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
#include "ndnrtc-namespace.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;
using namespace std::tr1;

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
pipelineTimer_(*EventWrapper::Create()),
needMoreFrames_(*EventWrapper::Create()),
modeSwitchedEvent_(*EventWrapper::Create()),
avSync_(nullptr),
keyNamespaceUsed_(true)
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

    if (frameBuffer_.isActivated())
        frameBuffer_.release();

    delete playoutBuffer_;
}

//******************************************************************************
#pragma mark - public
int NdnMediaReceiver::init(shared_ptr<Face> face)
{
    if (mode_ > ReceiverModeInit)
        return notifyError(RESULT_ERR, "can't initialize. stop receiver first");
    
    shared_ptr<string> streamPrefixString = NdnRtcNamespace::getStreamPrefix(params_);
    shared_ptr<string> deltaPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_);
    shared_ptr<string> keyPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_, true);
    
    if (!streamPrefixString.get() ||
        !deltaPrefixString.get() ||
        !keyPrefixString.get())
        return notifyError(RESULT_ERR, "producer frames prefixes \
                           were not provided");
    
    int bufSz = params_.bufferSize;
    int slotSz = params_.slotSize;
    
    interestTimeoutMs_ = params_.interestTimeout*1000;// params_.jitterSize*DeadlineBarrierCoeff; //interestTimeout*1000;
    
    if (RESULT_FAIL(frameBuffer_.init(bufSz, slotSz, params_.segmentSize)))
        return notifyError(RESULT_ERR, "could not initialize frame buffer");
    
    streamPrefix_ = Name(streamPrefixString->c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString->c_str());
    keyFramesPrefix_ = Name(keyPrefixString->c_str());
    
    producerSegmentSize_ = params_.segmentSize;
    face_ = face;
    currentProducerRate_ = params_.producerRate;
    
    if (RESULT_FAIL(playoutBuffer_->init(&frameBuffer_,
                                          currentProducerRate_,
                                         params_.jitterSize)))
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
    pipelinerDeltaFrameNo_ = 0;
    pipelinerKeyFrameNo_ = 0;
    
    switchToMode(ReceiverModeFlushed);
    
    // setup and start assembling thread
    unsigned int tid;
    
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
        frameBuffer_.release();
        return notifyError(RESULT_ERR, "media receiver was not started");
    }

    assemblingThread_.SetNotAlive();
    pipelineThread_.SetNotAlive();
    bookingThread_.SetNotAlive();
    playoutThread_.SetNotAlive();
    
    switchToMode(ReceiverModeInit);
    frameBuffer_.release();

    pipelineTimer_.StopTimer();
    pipelineTimer_.Set();
    needMoreFrames_.Set();
    
    assemblingThread_.Stop();
    pipelineThread_.Stop();
    bookingThread_.Stop();
    playoutThread_.Stop();
    
    jitterTiming_.stop();

    return RESULT_OK;
}

void NdnMediaReceiver::setLogger(NdnLogger *logger)
{
    LoggerObject::setLogger(logger);
    
    frameBuffer_.setLogger(logger);
    playoutBuffer_->setLogger(logger);
    jitterTiming_.setLogger(logger);
}

void NdnMediaReceiver::registerCallback(IMediaReceiverCallback *callback)
{
    callback_ = callback;
}

void NdnMediaReceiver::deregisterCallback()
{
    callback_ = nullptr;
}

void NdnMediaReceiver::triggerRebuffering()
{
    rebuffer(false);
}

//******************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnMediaReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    Name prefix = interest->getName();
    
    if (isStreamInterest(prefix))
    {
        bool isKey = NdnRtcNamespace::isKeyFramePrefix(prefix);
        PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(prefix);
        SegmentNumber segmentNo = NdnRtcNamespace::getSegmentNumber(prefix);
        
        if (!checkIsLate(prefix, nullptr, 0))
        {
            LOG_TRACE("timeout %s", prefix.toUri().c_str());
            frameBuffer_.notifySegmentTimeout(prefix);
        }
        else
        {
            LOG_TRACE("timeout for late %s", prefix.toUri().c_str());
            frameBuffer_.markSlotFree(prefix);
        }
    }
    else
        WARN("got timeout for unexpected prefix");
}

void NdnMediaReceiver::onSegmentData(const shared_ptr<const Interest>& interest,
                                     const shared_ptr<Data>& data)
{
    TRACE("on data");
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeter_);
    NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                       data->getContent().size());
    
    Name dataName = data->getName();
    TRACE("data: %s (size: %d, RTT: %d, SRTT: %f)", dataName.toUri().c_str(),
          data->getContent().size(), rtt_, srtt_);
    
    if (isStreamInterest(dataName))
    {
        bool isKey = NdnRtcNamespace::isKeyFramePrefix(dataName);
        int64_t frameNo = NdnRtcNamespace::getPacketNumber(dataName),
        segmentNo = NdnRtcNamespace::getSegmentNumber(dataName),
        finalSegmentNo = dataName[-1].toFinalSegment();
        
        if (frameNo >= 0 && segmentNo >= 0)
        {
            // frame numbers are negative inside frame buffer for key namespace
            if (checkExclusion(frameNo, isKey))
            {
                LOG_TRACE("exclude %s", dataName.toUri().c_str());
                return;
            }

            if (checkIsLate(dataName, (unsigned char*)data->getContent().buf(),
                            data->getContent().size()))
            {
                LOG_TRACE("late %s", dataName.toUri().c_str());
                return;
            }
            
            // append data to the buffer
            FrameBuffer::CallResult res = frameBuffer_.appendSegment(*data);
        }
        else
            WARN("bad data name %s", dataName.toUri().c_str());
    }
    else
        WARN("bad data name %s", dataName.toUri().c_str());
}

//******************************************************************************
#pragma mark - interfaces realization - IPlayoutBufferCallback
void NdnMediaReceiver::onFrameAddedToJitter(FrameBuffer::Slot *slot)
{
    unsigned int frameNo = slot->getFrameNumber();
    bool isKeyFrame = (slot->getFrameType() == webrtc::kKeyFrame);
    
    playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
    nReceived_++;
    emptyJitterCounter_ = 0;
    
    if (isKeyFrame)
    {
        nReceivedKey_++;
        keySegmentsNum_ += (double)slot->totalSegmentsNumber();
        nKeyFramesPending_--;
    }
    else
    {
        nReceivedDelta_++;
        deltaSegmentsNum_ += (double)slot->totalSegmentsNumber();
    }
    
    // extract current producer rate
    PacketData::PacketMetadata metadata;
    
    if (RESULT_GOOD(slot->getPacketMetadata(metadata)))
    {
        if (metadata.packetRate_ > 0 &&
            currentProducerRate_ != metadata.packetRate_)
        {
            currentProducerRate_ = metadata.packetRate_;
            
            int newJitterSize = NdnRtcUtils::toFrames(params_.jitterSize,
                                                      currentProducerRate_);
            
            playoutBuffer_->setMinJitterSize(newJitterSize);
            DBG("[RECEIVER] got updated producer rate %f. min jitter size %d",
                currentProducerRate_, playoutBuffer_->getMinJitterSize());
        }
    }
    
    // now check, whether we need more frames to request
    if (needMoreFrames())
    {
        TRACE("UNLOCKING needMoreFrames");
        needMoreFrames_.Set();
    }
    
    DBG("[RECEIVER] received frame %d (type: %s). jitter size: %d",
        frameNo, isKeyFrame?"KEY":"DELTA",
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
    
    nMissed_++;
    
    bool isInBuffer = (frameBuffer_.getState(frameNo) == FrameBuffer::Slot::StateAssembling);
    int nAssembled=0;
    int nTotal=1;
    bool isKeyFrame=0;
    
    if (isInBuffer)
    {
        shared_ptr<FrameBuffer::Slot> slot = frameBuffer_.getSlot(frameNo);
        
        if (slot.get())
        {
            isKeyFrame = slot->isKeyFrame();
            nAssembled = slot->assembledSegmentsNumber();
            nTotal = slot->totalSegmentsNumber();
        }
    }
    
    frameLogger_->log(NdnLoggerLevelInfo,"MISSED: \t%d \t \t \t%d \t%d \t%d \t%.2f (%d/%d)",
                      frameNo,
                      // empty
                      // empty
                      isKeyFrame,
                      playoutBuffer_->getJitterSize(),
                      isInBuffer,
                      (double)nAssembled/(double)nTotal, nAssembled, nTotal);
    
    shouldRequestFrame_ = true;
    needMoreFrames_.Set();
}
void NdnMediaReceiver::onPlayheadMoved(unsigned int nextPlaybackFrame,
                                       bool wasMissing)
{
    // now check, whether we need more frames to request
    if (needMoreFrames())
    {
        TRACE("UNLOCKING needMoreFrames");
        needMoreFrames_.Set();
    }
}
void NdnMediaReceiver::onJitterBufferUnderrun()
{
    TRACE("[PLAYOUT] UNDERRUN %d",
          emptyJitterCounter_+1);
    
    TRACE("UNLOCKING needMoreFrames (underrun)");
    shouldRequestFrame_ = true;
    needMoreFrames_.Set();
    
    emptyJitterCounter_++;
    
    if (emptyJitterCounter_ >= MaxUnderrunNum)
    {
        emptyJitterCounter_ = 0;
        rebuffer();
    }
}
#if 0
void NdnMediaReceiver::onFrameReachedDeadline(FrameBuffer::Slot *slot,
                           unordered_set<int> &lateSegments)
{
    for (unordered_set<int>::iterator it = lateSegments.begin();
         it != lateSegments.end(); ++it)
    {
        TRACE("[RECEIVER] slot %d needs retransmission for %d (is KEY: %s)",
                  slot->getFrameNumber(), *it, (slot->isKeyFrame())?"YES":"NO");
        requestSegment(slot->getFrameNumber(), *it);
    }
}
#endif

//******************************************************************************
#pragma mark - private
bool NdnMediaReceiver::processInterests()
{
    TRACE("[PIPELINER] LOCK on wait for event. new: %d free: %d assembling %d",
          frameBuffer_.getStat(FrameBuffer::Slot::StateNew),
          frameBuffer_.getStat(FrameBuffer::Slot::StateFree),
          frameBuffer_.getStat(FrameBuffer::Slot::StateAssembling));
    
    bool res = true;
    int eventsMask = FrameBuffer::Event::FirstSegment |
                        FrameBuffer::Event::Timeout;
    FrameBuffer::Event ev = frameBuffer_.waitForEvents(eventsMask);
    
    switch (ev.type_)
    {
        case FrameBuffer::Event::FirstSegment:
            res = onFirstSegmentReceived(ev);
            break;
            
        case FrameBuffer::Event::Timeout:
            res = onSegmentTimeout(ev);
            break;
            
        case FrameBuffer::Event::Error:
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
    
    if (mode_ != ReceiverModeChase)
    {
        int eventMask = FrameBuffer::Event::FreeSlot;
        
        TRACE("[BOOKING] LOCK on wait for free slots. free %d",
              frameBuffer_.getStat(FrameBuffer::Slot::StateFree));
        
        FrameBuffer::Event ev = frameBuffer_.waitForEvents(eventMask);
        
        switch (ev.type_) {
            case FrameBuffer::Event::FreeSlot:
                TRACE("on free slot %d %d", getJitterBufferSizeMs(),
                      getPipelinerBufferSizeMs());
                res = onFreeSlot(ev);
                break;
            
            case FrameBuffer::Event::Error:
                res = false;
                break;
            
            default:
                NDNERROR("got unexpected event: %d. ignoring", ev.type_);
                break;
        }
    }
    else
        modeSwitchedEvent_.Wait(WEBRTC_EVENT_INFINITE);
    
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
//            TRACE("process events");
            face_->processEvents();
        }
        catch(std::exception &e)
        {
            NDNERROR("got exception from ndn-library while processing events: %s",
                     e.what());
        }
        faceCs_.Leave();
    }
    else
        modeSwitchedEvent_.Wait(WEBRTC_EVENT_INFINITE);
    
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
            needMoreFrames_.Reset();
        }
            break;
            
        case ReceiverModeFlushed:
        {
            mode_ = mode;
            
            playoutBuffer_->flush();
            frameBuffer_.flush();
            needMoreFrames_.Set();
            
            INFO("switched to mode: Flushed");
        }
            break;
            
        case ReceiverModeChase:
        {
            mode_ = mode;
            INFO("switched to mode: Chase");
        }
            break;
            
        case ReceiverModeFetch:
        {
            mode_ = mode;
            playoutBuffer_->setMinJitterSize(NdnRtcUtils::
                                             toFrames(params_.jitterSize, currentProducerRate_));
            needMoreFrames_.Set();
            
            TRACE("set initial jitter size for %d ms (%d frames)",
                  params_.jitterSize,
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
    int nFetchAhead = getFetchAheadNumber(event.slot_->isKeyFrame());
    
    if (interestsNum <= nFetchAhead)
        return;
    
    TRACE("pipeline (frame %d)", event.slot_->getFrameNumber());
    
    // 2. setup frame prefix
    Name framePrefix = event.slot_->isKeyFrame() ?
                            keyFramesPrefix_ :
                                deltaFramesPrefix_;
    stringstream ss;
    
    ss << event.packetNo_;
    std::string frameNoStr = ss.str();
    
    framePrefix.addComponent((const unsigned char*)frameNoStr.c_str(),
                             frameNoStr.size());
    
    // 3. iteratively compute interest prefix and send out interest
    for (int i = nFetchAhead+1; i < event.slot_->totalSegmentsNumber(); i++)
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
    Name framesPrefix = deltaFramesPrefix_;
    Interest i(framesPrefix, interestTimeoutMs_);
    
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
    
    BookingId bookingId;
    frameBuffer_.bookSlot(framesPrefix, bookingId);
    expressInterest(i);
}

void NdnMediaReceiver::requestLatestKey()
{
    Interest i(keyFramesPrefix_, interestTimeoutMs_);
    
    i.setMinSuffixComponents(2);
    
    DBG("issuing key interest with exclusion: [*,%d]",
        keyExcludeFilter_);
    // seek for RIGHTMOST child with exclude wildcard [*,lastFrame]
    i.getExclude().appendAny();
    i.getExclude().appendComponent(NdnRtcUtils::componentFromInt(keyExcludeFilter_).getValue());
    i.setChildSelector(1);

    nKeyFramesPending_++;
    
    expressInterest(i);
}

void NdnMediaReceiver::requestSegment(PacketNumber frameNo,
                                      unsigned int segmentNo,
                                      bool useKeyNamespace)
{
    Name segmentPrefix = (useKeyNamespace)?keyFramesPrefix_:deltaFramesPrefix_;
    
    segmentPrefix.append(NdnRtcUtils::componentFromInt(frameNo));
    segmentPrefix.appendSegment(segmentNo);
    
    BookingId bookingId;
    frameBuffer_.bookSlot(segmentPrefix, bookingId);
    
    expressInterest(segmentPrefix);
}

bool NdnMediaReceiver::isStreamInterest(Name prefix)
{
    return streamPrefix_.match(prefix);
}

bool NdnMediaReceiver::isKeyInterest(Name prefix)
{
    return keyFramesPrefix_.match(prefix);
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
        
//        string iuri = i.getName().toUri();
//        
//        pendingInterests_[iuri] = pis;
//        pendingInterestsUri_[pis.interestID_] = iuri;
        
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

bool NdnMediaReceiver::isLate(const Name &prefix,
                              const unsigned char *segmentData, int dataSz)
{
    PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(prefix);

    return (frameNo < playoutBuffer_->getPlayheadPointer());
}

void NdnMediaReceiver::cleanupLateFrame(const Name &prefix)
{
    PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(prefix);
    
    TRACE("removing late frame %d from the buffer", frameNo);
    if (frameBuffer_.getState(frameNo) != FrameBuffer::Slot::StateFree)
        nLost_++;
    
    frameBuffer_.markSlotFree(prefix);
    
    if (frameNo < 0)
    {
        keyExcludeFilter_ = abs(frameNo);
        nKeyFramesPending_--;
        needMoreFrames_.Set();
        
        if (abs(pipelinerKeyFrameNo_) < abs(frameNo))
            pipelinerKeyFrameNo_ = abs(frameNo)+1;
    }
}

void NdnMediaReceiver::rebuffer(bool shouldNotify)
{
    DBG("*** REBUFFERING *** [#%d]", ++rebufferingEvent_);
    jitterTiming_.flush();
    rtt_ = 0;
    srtt_ = StartSRTT;
    nKeyFramesPending_ = 0;
    excludeFilter_ = pipelinerDeltaFrameNo_; //playoutBuffer_->getPlayheadPointer();
    switchToMode(ReceiverModeFlushed);
    needMoreFrames_.Reset();
    
    if (avSync_.get())
        avSync_->reset();
    
    if (shouldNotify && callback_)
        callback_->onRebuffer(this);
}

//NdnMediaReceiver::PendingInterestStruct
//NdnMediaReceiver::getPisForInterest(const string &iuri,
//                                    bool removeFromPITs)
//{
//    PendingInterestStruct pis = {(unsigned int)-1,(unsigned int)-1};
//    
//    pitCs_.Enter();
//    
//    if (pendingInterests_.find(iuri) != pendingInterests_.end())
//    {
//        pis = pendingInterests_[iuri];
//        
//        if (removeFromPITs)
//        {
//            pendingInterests_.erase(iuri);
//            pendingInterestsUri_.erase(pis.interestID_);
//        }
//    }
//    
//    pitCs_.Leave();
//    
//    return pis;
//}

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

bool NdnMediaReceiver::needMoreFrames(){
    bool pipelineBufferSmall = getPipelinerBufferSizeMs() < params_.jitterSize;
    bool needKeyFrames = needMoreKeyFrames();
    
    TRACE("need more frames? more key frames: %s pipeline small: %s "
          "underrun: %s",
          (needKeyFrames)?"YES":"NO",
          (pipelineBufferSmall)?"YES":"NO",
          (shouldRequestFrame_)?"YES":"NO");
    
    return (needKeyFrames ||
            pipelineBufferSmall ||
            shouldRequestFrame_) &&
            mode_ != ReceiverModeChase;
}

bool NdnMediaReceiver::needMoreKeyFrames(){
    // by default - return false
    return false;
}

int NdnMediaReceiver::getFetchAheadNumber(bool isKeyNamespace)
{
    int segNum = (isKeyNamespace)?keySegmentsNum_:deltaSegmentsNum_;
    int nReceived = (isKeyNamespace)?nReceivedKey_:nReceivedDelta_;
    
    return (nReceived == 0)?segNum:ceil((double)segNum/(double)nReceived);
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
            requestInitialSegment(); // request delta frame
            needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
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
                TRACE("LOCK needMoreFrames - buffers full");
                needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
            }
            else
            {
                bool isKey = needMoreKeyFrames();
                FrameNumber requestFrameNo = (isKey)?
                                                    pipelinerKeyFrameNo_:
                                                    pipelinerDeltaFrameNo_;
                
                DBG("[PIPELININIG] issue for %d (is KEY: %s), "
                    "jitter size ms: %d, in progress ms: %d",
                    requestFrameNo,
                    isKey ? "YES" : "NO",
                    jitterBufferSizeMs, pipelinerBufferSizeMs);
                frameLogger_->log(NdnLoggerLevelInfo,"PIPELINE: \t%d \t \t \t%d "
                                  "\t%d \t \t \t%.2f \t%d",
                                  requestFrameNo,
                                  // empty
                                  // empty
                                  isKey,
                                  playoutBuffer_->getJitterSize(),
                                  // empty
                                  // empty
                                  currentProducerRate_,
                                  pipelinerBufferSize_);
                
                int fetchAhead = getFetchAheadNumber(isKey);
                
                for (int i = 0; i <= fetchAhead; i++)
                {
                    requestSegment(requestFrameNo, i, isKey);
                }
                
                if (isKey)
                {
                    nKeyFramesPending_++;
                    pipelinerKeyFrameNo_++;
                }
                else
                    pipelinerDeltaFrameNo_++;
                
                if (shouldRequestFrame_)
                    shouldRequestFrame_ = false;
            }
        }
            break;
        default:
            TRACE("onFreeSlot: switch-default case");
            frameBuffer_.reuseEvent(event);
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
    TRACE("on first segment (%d-%d). frame type: %s",
          event.slot_->getFrameNumber(),
          event.segmentNo_, (event.segmentNo_ != 0)?
          "UNKNOWN": (event.slot_->isKeyFrame()?"KEY":"DELTA"));
    
    bool shouldPipeline = true;
    
    switch (mode_) {
        case ReceiverModeChase:
        {
            // need to filter out late frames if we are re-starting
            // in case of cold start - pipelinerFrameNo_ will be 0
            // otherwise - it should have stored last pipelined frame
            if (event.packetNo_ >= excludeFilter_)
            {
                TRACE("buffers flush");
                playoutBuffer_->flush();
                frameBuffer_.flush();
                
                isStartedKeyFetching_ = false;
                
                pipelinerDeltaFrameNo_ = event.packetNo_+
                        NdnRtcUtils::toFrames(rtt_/2., params_.producerRate);
                firstFrame_ = pipelinerDeltaFrameNo_;
                
                switchToMode(ReceiverModeFetch);
                DBG("[PIPELINER] start fetching from %d...", pipelinerDeltaFrameNo_);
            }
            else
                TRACE("[PIPELINER] receiving garbage after restart: %d-%d",
                      event.packetNo_, event.segmentNo_);
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
            if (event.packetNo_ == -1)
            {
                // clear filter if any
                excludeFilter_ = 0;
                DBG("[PIPELINER] got timeout for initial interest");
                requestInitialSegment();
            }
            else
                TRACE("[PIPELINER] timeout for %d while waiting for initial",
                      event.packetNo_);
        }
            break;
            
        case ReceiverModeFetch:
        {
//            if (!isLate(event.packetNo_, event.segmentNo_,  event.isKeyFrame_))
//            {
                // as we were issuing extra interests, need to check, whether
                // it is still make sens for re-issuing it
                if (event.slot_->getState() == FrameBuffer::Slot::StateNew ||
                    (event.slot_->getState() == FrameBuffer::Slot::StateAssembling &&
                     event.segmentNo_ < event.slot_->totalSegmentsNumber()))
                {
                    DBG("[PIPELINER] reissue %d-%d",
                        event.packetNo_, event.segmentNo_);
                    requestSegment(event.packetNo_, event.segmentNo_);
                }
            else
                DBG("[PIPELINER] skip timeout %d-%d", event.packetNo_,
                    event.segmentNo_);
//            }
//            else
//            {
//                DBG("got timeout for late frame %d-%d",
//                    event.frameNo_, event.segmentNo_);
//                if (frameBuffer_.getState(event.frameNo_) !=
//                    FrameBuffer::Slot::StateFree)
//                {
//                    frameLogger_->log(NdnLoggerLevelInfo, "\tTIMEOUT: \t%d \t%d \t%d \t%d \t%d",
//                                      event.frameNo_,
//                                      event.slot_->assembledSegmentsNumber(),
//                                      event.slot_->totalSegmentsNumber(),
//                                      event.slot_->isKeyFrame(),
//                                      playoutBuffer_->getJitterSize());
//                }
//                
//                cleanupLateFrame(event.frameNo_);
//            }
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


//******************************************************************************
bool NdnMediaReceiver::checkExclusion(PacketNumber frameNo, bool isKey)
{
    PacketNumber exclusion = (isKey)?keyExcludeFilter_:excludeFilter_;
    
    return (abs(frameNo) < exclusion);
}

bool NdnMediaReceiver::checkIsLate(const Name &prefix, const unsigned char *segmentData,
                                   int dataSz)
{
    if (isLate(prefix, segmentData, dataSz))
    {
        PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(prefix);
        SegmentNumber segmentNo = NdnRtcNamespace::getSegmentNumber(prefix);
        
        TRACE("late callback %d-%d", frameNo, segmentNo);
        cleanupLateFrame(prefix);

        if (NdnRtcNamespace::isKeyFramePrefix(prefix))
        {
            keyExcludeFilter_ = frameNo;
        }
    }
}
