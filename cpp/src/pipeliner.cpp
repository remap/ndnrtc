//
//  pipeliner.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author: Peter Gusev
//  Created: 2/11/14
//

#include "pipeliner.h"

using namespace ndnrtc;
using namespace std;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
InterestPipeliner::InterestPipeliner(ParamsStruct &params,
                                     FrameBuffer *frameBuffer,
                                     IPipelinerCallback *callback):
NdnRtcObject(params),
pipelineThread_(*ThreadWrapper::CreateThread(bufferProcessingRoutine, this)),
frameBookingThread_(*ThreadWrapper::CreateThread(interestPipelineRoutine, this)),
assemblerThread_(*ThreadWrapper::CreateThread(faceProcessingRoutine, this)),
generalCs_(*CriticalSectionWrapper::CreateCriticalSection()),
pendingInterests_(),
needMoreFrames_(*EventWrapper::Create()),
mode_(Created)
{
    resetData();
}

//******************************************************************************
#pragma mark - public
int InterestPipeliner::start(bool useKeyNamespace)
{
    if (RESULT_FAIL(init()))
    {
        notifyError("couldn't start pipeliner");
        return RESULT_ERR;
    }
    
    isRunning_ = true;
    useKeyNamespace_ = useKeyNamespace;
    
    unsigned int tid;
    int res = assemblerThread_.Start(tid);
    res |= frameBookingThread_.Start(tid);
    res |= pipelinerThread_.Start(tid);
    
    if (res != 0)
    {
        notifyError("couldn't start pipeliner");
        return RESULT_ERR;
    }
    
    interestFreqMeter_ = NdnRtcUtils::setupFrequencyMeter(10);
    segmentFreqMeter_ = NdnRtcUtils::setupFrequencyMeter(10);
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
    
    switchToMode(Flushed);
    return RESULT_OK;
}

int InterestPipeliner::stop()
{
    isRunning_ = false;
    
    pipelineThread_.SetNotAlive();
    pipelineThread_.Stop();
    frameBookingThread_.SetNotAlive();
    frameBookingThread_.Stop();
    assemblerThread_.SetNotAlive();
    assemblerThread_.Stop();
    
    resetData();
    
    NdnRtcUtils::releaseFrequencyMeter(interestFreqMeter_);
    NdnRtcUtils::releaseFrequencyMeter(segmentFreqMeter_);
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnMediaReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    Name prefix = interest->getName();
    string iuri = prefix.toUri();
    
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
}

void NdnMediaReceiver::onSegmentData(const shared_ptr<const Interest>& interest,
                                     const shared_ptr<Data>& data)
{
    TRACE("on data called");
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeter_);
    NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                       data->getContent().size());
    
    Name dataName = data->getName();
    string iuri =  (mode_ == Chase)? deltaPrefix_.toUri() : dataName.toUri();
    
    PendingInterestStruct pis = getPisForInterest(iuri, true);
    
    if (pis.interestID_ != (unsigned int)-1)
    {
        rtt_ = NdnRtcUtils::millisecondTimestamp()- pis.emissionTimestamp_;
        srtt_ = srtt_ + (rtt_-srtt_)*RttFilterAlpha;
    }
    
    TRACE("data: %s (size: %d, RTT: %d, SRTT: %f)", dataName.toUri().c_str(),
          data->getContent().size(), rtt_, srtt_);
    
    if (isStreamInterest(dataName))
    {
        bool isKeyNamespace = isFramesInterest(dataName, true);
        Name framesPrefix = (isKeyNamespace)?keyPrefix_:deltaPrefix_;
        
        int frameNo = NdnRtcUtils::frameNumber(dataName[-3]),
            segmentNo = NdnRtcUtils::segmentNumber(dataName[-2]),
            finalSegmentNo = dataName[-1].toFinalSegment();
        
        if (frameNo >= 0 && segmentNo >= 0)
        {
            if (isKeyNamespace)
                frameNo = - frameNo;
         
            callback_->onData(frameNo, segmentNo, finalSegmentNo, false,
                              data->getContent().getBuffer(),
                              data->getContent().size());
        }
        else
            callback_->onData(-1,-1, true, nullptr, 0);
    }
    else
        callback_->onData(-1,-1, true, nullptr, 0);
}

//******************************************************************************
// booking slots and pipelining interests
bool InterestPipeliner::pipelineInterests()
{
    bool res = true;
    int eventMask = FrameBuffer::Event::FreeSlot;
    FrameBuffer::Event ev = frameBuffer_->waitForEvents(eventMask);
    
    switch (ev.type_) {
        case FrameBuffer::Event::FreeSlot:
            res = onFreeSlot(ev);
            break;
            
        default:
            NDNERROR("unexpected buffer event: %d", ev.type_);
            break;
    }
    
    if (!needMoreFrames())
        needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
    
    return res;
}

// frame buffer events processing
bool InterestPipeliner::processBufferEvents()
{
    bool res = true;
    int eventsMask = FrameBuffer::Event::FirstSegment |
                        FrameBuffer::Event::Timeout;
    FrameBuffer::Event ev = frameBuffer_->waitForEvents(eventsMask);
    
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
            NDNERROR("unexpected buffer event: %d", ev.type_);
            break;
    }
    
    return res;
}

// processing face events
bool InterestPipeliner::processFaceEvents()
{
    try {
        faceWrapper_->processEvents();
    }
    catch(std::exception &e)
    {
        notifyError("ndn-cpp lib exception: %s", e.what());
    }
#warning sleep on timer instead
    usleep(10000);
    
    return isRunning_;
}

//******************************************************************************
// frame buffer events handlers
bool NdnMediaReceiver::onFreeSlot(FrameBuffer::Event &event)
{
    switch (mode_)
    {
        case Flushed:
        {
            DBG("[PIPELINING] issue for the latest frame");
            switchToMode(Chase);
            frameBuffer_->bookSlot(0);
            requestInitialSegment(false, exclude);
        }
            break;
            
        case ReceiverModeFetch:
        {
            // if current jitter buffer size + number of awaiting frames
            // is bigger than preferred size, skip pipelining
            if (!needMoreFrames())
            {
                DBG("[PIPELINING] not issuing further - pipeliner %d ms",
                    pipelinerBufferSizeMs);
                
                frameBuffer_.reuseEvent(event);
                needMoreFrames_.Wait(WEBRTC_EVENT_INFINITE);
            }
            else
            {
                //                DBG("[PIPELININIG] issue for %d (%d frames ahead), pipeliner ms: %d",
                //                    pipelinerFrameNo_,
                //                    (playoutBuffer_->getState() == PlayoutBuffer::StatePlayback)?
                //                    pipelinerFrameNo_ - playoutBuffer_->getPlayheadPointer() :
                //                    pipelinerFrameNo_ - firstFrame_,
                //                    pipelinerBufferSizeMs);
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
    
    return isRunning_;
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

//******************************************************************************
void InterestPipeliner::init()
{
    string initialPrefix;
    
    if (RESULT_NOT_OK(MediaSender::getStreamPrefix(params_,
                                                   initialPrefix)))
        return notifyError(RESULT_ERR, "producer frames prefix \
                           was not provided");
    
    streamPrefix_ = Name(initialPrefix.c_str());
    
    string keyFramesPrefix, deltaFramesPrefix;
    
    MediaSender::getStreamFramePrefix(params_, deltaFramesPrefix, true);
    MediaSender::getStreamFramePrefix(params_, keyFramesPrefix, true);
    
    keyPrefix_ = Name(keyFramesPrefix.c_str());
    deltaPrefix_ = Name(deltaFramesPrefix.c_str());
}

void InterestPipeliner::resetData()
{
    switchToMode(Init);
    useKeyNamespace_ = false;
    exclude_ = -1;
    deltaFrameNo_ = 0;
    keyFrameNo_ = 0;
    fetchAhead_ = 0;
}

string InterestPipeliner::modeToString(Mode mode)
{
    static string *modes[] = {
        [Created]   = "Created",
        [Init]      = "Init",
        [Flushed]   = "Flushed",
        [Chase]     = "Chase",
        [Fetch]     = "Fetch"
    };
    
    return modes[mode];
}

void InterestPipeliner::switchToMode(Mode newMode)
{
    bool check = true;
    
    switch (mode) {
        case Init:
        case Flushed:
        case ReceiverModeChase:
        case ReceiverModeFetch:
            break;
        case Created:
        {
            check = false;
            NDNERROR("illegal switch to mode: Created");
        }
            break;
        default:
            check = false;
            WARN("unable to switch to unknown mode %d", mode);
            break;
    }
    
    if (check)
    {
        mode_ = mode
        INFO("switched to mode: %s", modeToString(mode_).c_str());
    }
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
    return streamPrefix_.match(prefix);
}
bool NdnMediaReceiver::isFramesInterest(Name prefix, bool checkForKeyNamespace)
{
    if (checkForKeyNamespace)
        return keyPrefix_.match(prefix);
    else
        return deltaPrefix_.match(prefix);
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

unsigned int NdnMediaReceiver::getPipelinerBufferSizeMs()
{
    CriticalSectionScoped scopedCs(&dataCs_);
    
    int framesAssembling = frameBuffer_.getStat(FrameBuffer::Slot::StateAssembling);
    pipelinerBufferSize_ = frameBuffer_.getStat(FrameBuffer::Slot::StateNew) +
    framesAssembling;
    
    return NdnRtcUtils::toTimeMs(pipelinerBufferSize_,
                                 currentProducerRate_);
}
