//
//  media-receiver.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

//#undef NDN_LOGGING
#undef NDN_DETAILED

#define OUTSTANDING_K 2

#include "media-receiver.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

const double ndnrtc::RateDeviation = 0.1; // 10%
const double ndnrtc::RttFilterAlpha = 0.01;
const double ndnrtc::RateFilterAlpha = 0.3;
const unsigned int ndnrtc::RebufferThreshold = 3000; // if N seconds were no frames - rebuffer

//******************************************************************************
#pragma mark - construction/destruction
NdnMediaReceiver::NdnMediaReceiver(const ParamsStruct &params) :
NdnRtcObject(params),
pipelineThread_(*ThreadWrapper::CreateThread(pipelineThreadRoutine, this)),
bookingThread_(*ThreadWrapper::CreateThread(bookingThreadRoutine, this)),
assemblingThread_(*ThreadWrapper::CreateThread(assemblingThreadRoutine, this)),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this, kRealtimePriority)),
faceCs_(*CriticalSectionWrapper::CreateCriticalSection()),
pitCs_(*CriticalSectionWrapper::CreateCriticalSection()),
dataCs_(*CriticalSectionWrapper::CreateCriticalSection()),
mode_(ReceiverModeCreated),
face_(nullptr),
pendingInterests_(),
pipelineTimer_(*EventWrapper::Create()),
needMoreFrames_(*EventWrapper::Create()),
modeSwitchedEvent_(*EventWrapper::Create()),
avSync_(nullptr)
{
    interestFreqMeter_ = NdnRtcUtils::setupFrequencyMeter(10);
    segmentFreqMeter_ = NdnRtcUtils::setupFrequencyMeter(10);
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
}

NdnMediaReceiver::~NdnMediaReceiver()
{
    if (mode_ >= ReceiverModeFlushed)
        stopFetching();
    
    NdnRtcUtils::releaseFrequencyMeter(interestFreqMeter_);
    NdnRtcUtils::releaseFrequencyMeter(segmentFreqMeter_);
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
    delete playoutBuffer_;
}

//******************************************************************************
#pragma mark - public
int NdnMediaReceiver::init(shared_ptr<Face> face)
{
    if (mode_ > ReceiverModeInit)
        return notifyError(RESULT_ERR, "can't initialize. stop receiver first");
    
    string initialPrefix;
    
    if (RESULT_NOT_OK(MediaSender::getStreamFramePrefix(params_,
                                                        initialPrefix)))
        return notifyError(RESULT_ERR, "producer frames prefix \
                           was not provided");
    
    int bufSz = params_.bufferSize;
    int slotSz = params_.slotSize;
    
    interestTimeoutMs_ = params_.interestTimeout*1000;
    
    if (RESULT_FAIL(frameBuffer_.init(bufSz, slotSz)))
        return notifyError(RESULT_ERR, "could not initialize frame buffer");
    
    framesPrefix_ = Name(initialPrefix.c_str());
    producerSegmentSize_ = params_.segmentSize;
    face_ = face;
    currentProducerRate_ = params_.producerRate;
    
    if (RESULT_FAIL(playoutBuffer_->init(&frameBuffer_,
                                          params_.jitterSize,
                                          currentProducerRate_)))
        return notifyError(RESULT_ERR, "could not initialize playout buffer");
    
    jitterTiming_.flush();
    playoutBuffer_->registerBufferCallback(this);
    switchToMode(ReceiverModeInit);
    
    return RESULT_OK;
}

int NdnMediaReceiver::startFetching()
{
    if (mode_ != ReceiverModeInit)
        return notifyError(RESULT_ERR, "receiver is not initialized or \
                           has been already started");
    
    int res = RESULT_OK;
    pipelinerFrameNo_ = 0;
    
    switchToMode(ReceiverModeFlushed);
    
    // setup and start assembling thread
    unsigned int tid = ASSEMBLING_THREAD_ID;
    
    if (!assemblingThread_.Start(tid))
    {
        notifyError(RESULT_ERR, "can't start assembling thread");
        res = RESULT_ERR;
    }
    
    // setup and start pipeline thread - twice faster than producer rate
    unsigned long pipelinerTimerInterval = 1000/ParamsStruct::validate(params_.producerRate, 1,
                                                                       MaxFrameRate,
                                                                       res,
                                                                       DefaultParams.producerRate);
    
    pipelineTimer_.StartTimer(true, pipelinerTimerInterval);
    
    tid = PIPELINER_THREAD_ID;
    if (!pipelineThread_.Start(tid))
    {
        notifyError(RESULT_ERR, "can't start playout thread");
        res = RESULT_ERR;
    }
    else
        if (!bookingThread_.Start(tid))
        {
            notifyError(RESULT_ERR, "can't start booking thread");
            res = RESULT_ERR;
        }
    
    if (RESULT_FAIL(res))
        stopFetching();
    
    if (!playoutThread_.Start(tid))
    {
        notifyError(RESULT_ERR, "can't start playout thread");
        res = RESULT_ERR;
    }
    
    return res;
}

int NdnMediaReceiver::stopFetching()
{
    TRACE("stop fetching");
    
    if (mode_ < ReceiverModeFlushed)
    {
        TRACE("return on init");
        return notifyError(RESULT_ERR, "media receiver was not started");
    }
    
    pipelineTimer_.StopTimer();
    pipelineTimer_.Set();
    needMoreFrames_.Set();
    
    assemblingThread_.SetNotAlive();
    pipelineThread_.SetNotAlive();
    bookingThread_.SetNotAlive();
    TRACE("release buf");
    frameBuffer_.release();
    
    assemblingThread_.Stop();
    pipelineThread_.Stop();
    bookingThread_.Stop();
    TRACE("stopped threads");
    
    jitterTiming_.stop();
    modeSwitchedEvent_.Set();
    playoutThread_.SetNotAlive();
    playoutThread_.Stop();
    modeSwitchedEvent_.Reset();
    
    switchToMode(ReceiverModeInit);
    return RESULT_OK;
}

void NdnMediaReceiver::setLogger(NdnLogger *logger)
{
    logger_ = logger;
    
    frameBuffer_.setLogger(logger);
    playoutBuffer_->setLogger(logger);
}

//******************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnMediaReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    Name prefix = interest->getName();
    string iuri = prefix.toUri();
    PendingInterestStruct pis = getPisForInterest(iuri, true);
    
    if (isStreamInterest(prefix))
    {
        unsigned int frameNo = 0, segmentNo = 0;
        
        // check if it's not a first segment
        if (prefix.getComponentCount() > framesPrefix_.getComponentCount())
        {
            unsigned int nComponents = prefix.getComponentCount();
            
            frameNo =
            NdnRtcUtils::frameNumber(prefix.getComponent(nComponents-2));
            segmentNo =
            NdnRtcUtils::segmentNumber(prefix.getComponent(nComponents-1));
        }
        else
            TRACE("timeout for first interest");
        
        frameBuffer_.notifySegmentTimeout(frameNo, segmentNo);
    }
    else
        WARN("got timeout for unexpected prefix");
}

void NdnMediaReceiver::onSegmentData(const shared_ptr<const Interest>& interest,
                                     const shared_ptr<Data>& data)
{
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeter_);
    NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                       data->getContent().size());
    
    Name prefix = data->getName();
    string iuri =  (mode_ == ReceiverModeChase)?
    framesPrefix_.toUri() :
    prefix.toUri();
    PendingInterestStruct pis = getPisForInterest(iuri, true);
    
    if (pis.interestID_ != (unsigned int)-1)
    {
        rtt_ = NdnRtcUtils::millisecondTimestamp()- pis.emissionTimestamp_;
        srtt_ = srtt_ + (rtt_-srtt_)*RttFilterAlpha;
    }
    
    
    TRACE("data: %s (size: %d, RTT: %d, SRTT: %f)", prefix.toUri().c_str(),
          data->getContent().size(), rtt_, srtt_);
    
    if (isStreamInterest(prefix))
    {
        unsigned int nComponents = prefix.getComponentCount();
        long frameNo =
        NdnRtcUtils::frameNumber(prefix.getComponent(framesPrefix_.getComponentCount())),
        
        segmentNo =
        NdnRtcUtils::segmentNumber(prefix.getComponent(framesPrefix_.getComponentCount()+1));
        
        if (frameNo >= 0 && segmentNo >= 0)
        {
            if (isLate(frameNo))
            {
                TRACE("got data for late frame %d-%d", frameNo, segmentNo);
                if (frameBuffer_.getState(frameNo) !=
                    FrameBuffer::Slot::StateFree)
                {
                    frameLogger_->log(NdnLoggerLevelInfo, "\tLATE: \t%d", frameNo);
                }
                cleanupLateFrame(frameNo);
            }
            else
            {
                playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
                
                // check if it is a very first segment in a stream
                if (frameNo >= excludeFilter_)
                {
                    if (mode_ == ReceiverModeChase)
                    {
                        TRACE("got first segment of size %d. rename slot: %d->%d",
                              data->getContent().size(),
                              0, frameNo);
                        
                        frameBuffer_.renameSlot(0, frameNo);
                    }
                    
                    // check if it's a first segment
                    if (frameBuffer_.getState(frameNo) ==
                        FrameBuffer::Slot::StateNew)
                    {
                        // get total number of segments
                        Name::Component finalBlockID =
                        data->getMetaInfo().getFinalBlockID();
                        
                        // final block id stores the number of the last segment, so
                        //  the number of all segments is greater by 1
                        unsigned int segmentsNum =
                        NdnRtcUtils::segmentNumber(finalBlockID)+1;
                        
                        frameBuffer_.markSlotAssembling(frameNo, segmentsNum,
                                                        producerSegmentSize_);
                    }
                    
                    // finally, append data to the buffer
                    FrameBuffer::CallResult res =
                    frameBuffer_.appendSegment(frameNo, segmentNo,
                                               data->getContent().size(),
                                               (unsigned char*)data->getContent().buf());
                }
                else
                    DBG("garbage (%d-%d)", frameNo, segmentNo);
            }
        }
        else
        {
            WARN("got bad frame/segment numbers: %d (%d)", frameNo, segmentNo);
        }
    }
    else
        WARN("got data with unexpected prefix");
}

//******************************************************************************
#pragma mark - intefaces realization - IPlayoutBufferCallback
void NdnMediaReceiver::onFrameAddedToJitter(FrameBuffer::Slot *slot)
{
    unsigned int frameNo = slot->getFrameNumber();
    webrtc::VideoFrameType type = slot->getFrameType();
    
    // extract current producer rate
    PacketData::PacketMetadata metadata;
    
    if (RESULT_GOOD(slot->getPacketMetadata(metadata)))
    {
        if (metadata.packetRate_ > 0 &&
            currentProducerRate_ != metadata.packetRate_)
        {
            currentProducerRate_ = metadata.packetRate_;
            
            int newJitterSize = NdnRtcUtils::toFrames(MinJitterSizeMs,
                                                      currentProducerRate_);
            
            playoutBuffer_->setMinJitterSize(newJitterSize);
            DBG("[RECEIVER] got updated producer rate %f. min jitter size %d",
                currentProducerRate_, playoutBuffer_->getMinJitterSize());
        }
    }
    
    playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
    nReceived_++;
    emptyJitterCounter_ = 0;
    
    // now check, whether we need more frames to request
    if (needMoreFrames())
        needMoreFrames_.Set();
    
    DBG("[RECEIVER] received frame %d (type: %s). jitter size: %d",
        frameNo, (type == webrtc::kKeyFrame)?"KEY":"DELTA",
        playoutBuffer_->getJitterSize());
    
    frameLogger_->log(NdnLoggerLevelInfo,"ADDED: \t%d \t \t \t \t%d \t \t \t%.2f \t%d",
                      frameNo,
                      // empty
                      // empty
                      // empty
                      playoutBuffer_->getJitterSize(),
                      // empty
                      // empty
                      currentProducerRate_,
                      pipelinerBufferSize_);
}
void NdnMediaReceiver::onBufferStateChanged(PlayoutBuffer::State newState)
{
    playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
}
void NdnMediaReceiver::onMissedFrame(unsigned int frameNo)
{
    TRACE("[PLAYOUT] missed %d", frameNo);
    
    if (lastMissedFrameNo_ != frameNo)
    {
        lastMissedFrameNo_ = frameNo;
        nMissed_++;
    }
    
    frameLogger_->log(NdnLoggerLevelInfo,"MISSED: \t%d \t \t \t \t%d",
                      frameNo,
                      // empty
                      // empty
                      // empty
                      playoutBuffer_->getJitterSize());
}
void NdnMediaReceiver::onPlayheadMoved(unsigned int nextPlaybackFrame)
{
    // now check, whether we need more frames to request
    if (needMoreFrames())
        needMoreFrames_.Set();
}
void NdnMediaReceiver::onJitterBufferUnderrun()
{
    TRACE("[PLAYOUT] UNDERRUN %d",
          emptyJitterCounter_+1);
    
    emptyJitterCounter_++;
    
    if (emptyJitterCounter_ >= MaxUnderrunNum)
    {
        emptyJitterCounter_ = 0;
        rebuffer();
    }
}

//******************************************************************************
#pragma mark - private
bool NdnMediaReceiver::processInterests()
{
    //    TRACE("wait event. size %d", frameBuffer_.getStat(FrameBuffer::Slot::StateFree));
    
    bool res = true;
    FrameBuffer::Event ev = frameBuffer_.waitForEvents(pipelinerEventsMask_);
    
    switch (ev.type_)
    {
        case FrameBuffer::Event::EventTypeFirstSegment:
            res = onFirstSegmentReceived(ev);
            break;
            
        case FrameBuffer::Event::EventTypeTimeout:
            res = onSegmentTimeout(ev);
            break;
            
        case FrameBuffer::Event::EventTypeError:
            res = onError(ev);
            break;
        default:
            NDNERROR("got unexpected event: %d. ignoring", ev.type_);
            break;
    }
    
    //    TRACE("res %d", res);
    return res;
}

bool NdnMediaReceiver::processBookingSlots()
{
    bool res = true;
    int eventMask = FrameBuffer::Event::EventTypeFreeSlot;
    
    FrameBuffer::Event ev = frameBuffer_.waitForEvents(eventMask);
    
    switch (ev.type_) {
        case FrameBuffer::Event::EventTypeFreeSlot:
            TRACE("on free slot %d %d", getJitterBufferSizeMs(),
                  getPipelinerBufferSizeMs());
            res = onFreeSlot(ev);
            break;
            
        default:
            NDNERROR("got unexpected event: %d. ignoring", ev.type_);
            break;
    }
    
    return res;
}

bool NdnMediaReceiver::processAssembling()
{
    // check if the first interest has been sent out
    // if not - skip processing events
    if (mode_ >= ReceiverModeChase)
    {
        faceCs_.Enter();
        try {
            face_->processEvents();
        }
        catch(std::exception &e)
        {
            NDNERROR("got exception from ndn-library while processing events: %s",
                     e.what());
        }
        faceCs_.Leave();
    }
    
    usleep(10000);
    
    return true;
}

bool NdnMediaReceiver::processPlayout()
{
    if (mode_ == ReceiverModeFetch)
    {
        uint64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (now - playoutLastUpdate_ >= RebufferThreshold)
            rebuffer();
        else if (playoutBuffer_->getState() == PlayoutBuffer::StatePlayback)
            playbackPacket(now);
    }
    else
        modeSwitchedEvent_.Wait(WEBRTC_EVENT_INFINITE);
    
    return true;
}

void NdnMediaReceiver::switchToMode(NdnMediaReceiver::ReceiverMode mode)
{
    bool check = true;
    
    switch (mode) {
        case ReceiverModeCreated:
        {
            NDNERROR("illegal switch to mode: Created");
        }
            break;
            
        case ReceiverModeInit:
        {
            mode_ = mode;
            // setup pipeliner thread params
//            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot;            
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFirstSegment |
                                    FrameBuffer::Event::EventTypeTimeout;
            needMoreFrames_.Reset();
        }
            break;
            
        case ReceiverModeFlushed:
        {
            mode_ = mode;

//            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot;
            
            playoutBuffer_->flush();
            frameBuffer_.flush();
            needMoreFrames_.Set();
            
            INFO("switched to mode: Flushed");
        }
            break;
            
        case ReceiverModeChase:
        {
            mode_ = mode;
//            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFirstSegment |
//            FrameBuffer::Event::EventTypeTimeout;
            INFO("switched to mode: Chase");
        }
            break;
            
        case ReceiverModeFetch:
        {
            mode_ = mode;
//            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot |
//                                    FrameBuffer::Event::EventTypeFirstSegment |
//                                      FrameBuffer::Event::EventTypeTimeout;
            
            playoutBuffer_->setMinJitterSize(NdnRtcUtils::
                                             toFrames(MinJitterSizeMs, currentProducerRate_));
            needMoreFrames_.Set();
            
            TRACE("set initial jitter size for %d ms (%d frames)",
                  MinJitterSizeMs,
                  playoutBuffer_->getMinJitterSize());
            INFO("switched to mode: Fetch");
        }
            break;
            
        default:
            check = false;
            WARN("unable to switch to unknown mode %d", mode);
            break;
    }
    
    modeSwitchedEvent_.Set();
}

void NdnMediaReceiver::pipelineInterests(FrameBuffer::Event &event)
{
    // 1. get number of interests to be send out
    int interestsNum = event.slot_->totalSegmentsNumber() - 1;
    
    if (interestsNum <= fetchAhead_)
        return;
    
    TRACE("pipeline (frame %d)", event.frameNo_);
    
    // 2. setup frame prefix
    Name framePrefix = framesPrefix_;
    stringstream ss;
    
    ss << event.frameNo_;
    std::string frameNoStr = ss.str();
    
    framePrefix.addComponent((const unsigned char*)frameNoStr.c_str(),
                             frameNoStr.size());
    
    // 3. iteratively compute interest prefix and send out interest
    for (int i = fetchAhead_+1; i < event.slot_->totalSegmentsNumber(); i++)
        // send out interests only for segments we don't have yet
        if (i != event.segmentNo_)
        {
            Name segmentPrefix = framePrefix;
            
            segmentPrefix.appendSegment(i);
            expressInterest(segmentPrefix);
        }
}

void NdnMediaReceiver::requestInitialSegment()
{
    Interest i(framesPrefix_, interestTimeoutMs_);
    
    i.setMinSuffixComponents(2);
    
    // if we have exclude filter set up - apply it
    if (excludeFilter_ != 0)
    {
        DBG("issuing initial interest with exclusion: [*,%d]",
            excludeFilter_);
        // seek for RIGHTMOST child with exclude wildcard [*,lastFrame]
        i.getExclude().appendAny();
        i.getExclude().appendComponent(NdnRtcUtils::componentFromInt(excludeFilter_).getValue());
        i.setChildSelector(1);
    }
    else
    {
        DBG("issuing for rightmost");
        // otherwise - seek for RIGHTMOST child
        i.setChildSelector(1);
    }
    
    expressInterest(i);
}

void NdnMediaReceiver::requestSegment(unsigned int frameNo,
                                      unsigned int segmentNo)
{
    Name segmentPrefix = framesPrefix_;
    
    segmentPrefix.append(NdnRtcUtils::componentFromInt(frameNo));
    segmentPrefix.appendSegment(segmentNo);
    
    expressInterest(segmentPrefix);
}

bool NdnMediaReceiver::isStreamInterest(Name prefix)
{
    return framesPrefix_.match(prefix);
}

void NdnMediaReceiver::expressInterest(Name &prefix)
{
    Interest i(prefix, interestTimeoutMs_); // 4*srtt_);
    expressInterest(i);
}

void NdnMediaReceiver::expressInterest(Interest &i)
{
    TRACE("interest %s", i.getName().toUri().c_str());
    faceCs_.Enter();
    try {
        PendingInterestStruct pis = {0,0};
        
        pis.emissionTimestamp_ = NdnRtcUtils::millisecondTimestamp();
        pis.interestID_ = face_->expressInterest(i,
                                                 bind(&NdnMediaReceiver::onSegmentData, this, _1, _2),
                                                 bind(&NdnMediaReceiver::onTimeout, this, _1));
        
        string iuri = i.getName().toUri();
        
        pendingInterests_[iuri] = pis;
        pendingInterestsUri_[pis.interestID_] = iuri;
        
        NdnRtcUtils::frequencyMeterTick(interestFreqMeter_);
    }
    catch(std::exception &e)
    {
        NDNERROR("got exception from ndn-library: %s \
                 (while expressing interest: %s)",
                 e.what(),i.getName().toUri().c_str());
    }
    faceCs_.Leave();
}

bool NdnMediaReceiver::isLate(unsigned int frameNo)
{
    return false;
}

void NdnMediaReceiver::cleanupLateFrame(unsigned int frameNo)
{
    TRACE("removing late frame %d from the buffer");
    if (frameBuffer_.getState(frameNo) != FrameBuffer::Slot::StateFree)
        nLost_++;
    
    frameBuffer_.markSlotFree(frameNo);
}

void NdnMediaReceiver::rebuffer()
{
    DBG("*** REBUFFERING *** [#%d]", ++rebufferingEvent_);
    jitterTiming_.flush();
    rtt_ = 0;
    srtt_ = StartSRTT;
    excludeFilter_ = pipelinerFrameNo_;
    switchToMode(ReceiverModeFlushed);
    needMoreFrames_.Reset();
    
    if (avSync_.get())
        avSync_->reset();
}

NdnMediaReceiver::PendingInterestStruct
NdnMediaReceiver::getPisForInterest(const string &iuri,
                                    bool removeFromPITs)
{
    PendingInterestStruct pis = {(unsigned int)-1,(unsigned int)-1};
    
    pitCs_.Enter();
    
    if (pendingInterests_.find(iuri) != pendingInterests_.end())
    {
        pis = pendingInterests_[iuri];
        
        if (removeFromPITs)
        {
            pendingInterests_.erase(iuri);
            pendingInterestsUri_.erase(pis.interestID_);
        }
    }
    
    pitCs_.Leave();
    
    return pis;
}

unsigned int NdnMediaReceiver::getJitterBufferSizeMs()
{
    CriticalSectionScoped scopedCs(&dataCs_);
    
    return NdnRtcUtils::toTimeMs(playoutBuffer_->getJitterSize(),
                                 currentProducerRate_);
}

unsigned int NdnMediaReceiver::getPipelinerBufferSizeMs()
{
    CriticalSectionScoped scopedCs(&dataCs_);
    
    int framesAssembling = frameBuffer_.getStat(FrameBuffer::Slot::StateAssembling);
    pipelinerBufferSize_ = frameBuffer_.getStat(FrameBuffer::Slot::StateNew) +
    framesAssembling;
    
    return NdnRtcUtils::toTimeMs(pipelinerBufferSize_,
                                 currentProducerRate_);
}

//******************************************************************************
// frame buffer events for pipeliner
bool NdnMediaReceiver::onFreeSlot(FrameBuffer::Event &event)
{
    bool stop = false;
    
    switch (mode_)
    {
        case ReceiverModeFlushed:
        {
            DBG("[PIPELINING] issue for the latest frame");
            switchToMode(ReceiverModeChase);
            frameBuffer_.bookSlot(0);
            requestInitialSegment();
        }
            break;
            
        case ReceiverModeFetch:
        {
            unsigned pipelinerBufferSizeMs = getPipelinerBufferSizeMs();
            unsigned jitterBufferSizeMs = getJitterBufferSizeMs();
            
            // if current jitter buffer size + number of awaiting frames
            // is bigger than preferred size, skip pipelining
            if (!needMoreFrames())
            {
                DBG("[PIPELINING] not issuing further - jitter %d ms, "
                    "pipeliner %d ms",
                    jitterBufferSizeMs,
                    pipelinerBufferSizeMs);
                
                frameBuffer_.reuseEvent(event);
                needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
            }
            else
            {
                DBG("[PIPELININIG] issue for %d (%d frames ahead), jitter size ms: %d, in progress ms: %d",
                    pipelinerFrameNo_,
                    (playoutBuffer_->getState() == PlayoutBuffer::StatePlayback)?
                    pipelinerFrameNo_ - playoutBuffer_->getPlayheadPointer() :
                    pipelinerFrameNo_ - firstFrame_,
                    jitterBufferSizeMs, pipelinerBufferSizeMs);
                
                frameLogger_->log(NdnLoggerLevelInfo,"PIPELINE: \t%d \t \t \t "
                                  "\t%d \t \t \t%.2f \t%d",
                                  pipelinerFrameNo_,
                                  // empty
                                  // empty
                                  // empty
                                  playoutBuffer_->getJitterSize(),
                                  // empty
                                  // empty
                                  currentProducerRate_,
                                  pipelinerBufferSize_);
                
                frameBuffer_.bookSlot(pipelinerFrameNo_);
                
                for (int i = 0; i <= fetchAhead_; i++)
                {
                    requestSegment(pipelinerFrameNo_, i);
                }
                
                pipelinerFrameNo_++;
            }
        }
            break;
            
        case ReceiverModeChase:
        {
            TRACE("in chase mode - waiting for more frames needed");
            needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
        }
            break;
        default:
            break;
    }
    
    return !stop;
}

bool NdnMediaReceiver::onFirstSegmentReceived(FrameBuffer::Event &event)
{
    // when we are in the fetch mode - the first segment received should have
    // number 0 (as we requested it)
    // in ReceiverModeChase mode we can receive whatever
    // segment number (as we requested right most child without knowing
    // frame number, nor segment number)
    TRACE("on first segment (%d-%d). frame type: %s", event.frameNo_,
          event.segmentNo_, (event.segmentNo_ != 0)?
          "UNKNOWN": (event.slot_->isKeyFrame()?"KEY":"DELTA"));
    
    bool shouldPipeline = true;
    
    switch (mode_) {
        case ReceiverModeChase:
        {
            // need to filter out late frames if we are re-starting
            // in case of cold start - pipelinerFrameNo_ will be 0
            // otherwise - it should have stored last pipelined frame
            if (event.frameNo_ >= excludeFilter_)
            {
                TRACE("buffers flush");
                playoutBuffer_->flush();
                frameBuffer_.flush();
                
                pipelinerFrameNo_ = event.frameNo_+ NdnRtcUtils::toFrames(rtt_/2., params_.producerRate);
                firstFrame_ = pipelinerFrameNo_;
                
                switchToMode(ReceiverModeFetch);
                DBG("[PIPELINER] start fetching from %d...", pipelinerFrameNo_);
            }
            else
                TRACE("[PIPELINER] receiving garbage after restart: %d-%d",
                      event.frameNo_, event.segmentNo_);
        }
            break;
            
        case ReceiverModeFetch:
        {
            pipelineInterests(event);
        }
            break;
            
        default:
            break;
    }
    
    return true;
}

bool NdnMediaReceiver::onSegmentTimeout(FrameBuffer::Event &event)
{
    switch (mode_){
        case ReceiverModeChase:
        {
            if (event.frameNo_ == 0)
            {
                // clear filter if any
                excludeFilter_ = 0;
                DBG("[PIPELINER] got timeout for initial interest");
                requestInitialSegment();
            }
            else
                TRACE("[PIPELINER] got timeout for frame %d while waiting for initial",
                      event.frameNo_);
        }
            break;
            
        case ReceiverModeFetch:
        {
            if (!isLate(event.frameNo_))
            {
                // as we were issuing extra interests, need to check, whether
                // it is still make sens for re-issuing it
                if (event.slot_->getState() == FrameBuffer::Slot::StateNew ||
                    (event.slot_->getState() == FrameBuffer::Slot::StateAssembling &&
                     event.segmentNo_ < event.slot_->totalSegmentsNumber()))
                {
                    DBG("[PIPELINER] REISSUE %d-%d",
                        event.frameNo_, event.segmentNo_);
                    requestSegment(event.frameNo_,event.segmentNo_);
                }
            }
            else
            {
                DBG("got timeout for late frame %d-%d",
                    event.frameNo_, event.segmentNo_);
                if (frameBuffer_.getState(event.frameNo_) !=
                    FrameBuffer::Slot::StateFree)
                {
                    frameLogger_->log(NdnLoggerLevelInfo, "\tTIMEOUT: \t%d \t%d \t%d \t%d \t%d",
                                      event.frameNo_,
                                      event.slot_->assembledSegmentsNumber(),
                                      event.slot_->totalSegmentsNumber(),
                                      event.slot_->isKeyFrame(),
                                      playoutBuffer_->getJitterSize());
                }
                
                cleanupLateFrame(event.frameNo_);
            }
        }
            break;
            
        default:
            break;
    }
    
    return true;
}

bool NdnMediaReceiver::onError(FrameBuffer::Event &event)
{
    NDNERROR("got error event on frame buffer");
    return false; // stop pipelining
}

