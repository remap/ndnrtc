//
//  pipeliner.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "pipeliner.h"
#include "ndnrtc-namespace.h"
#include "rtt-estimation.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

const double Pipeliner::SegmentsAvgNumDelta = 8.;
const double Pipeliner::SegmentsAvgNumKey = 25.;

//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::Pipeliner::Pipeliner(const shared_ptr<Consumer> &consumer):
NdnRtcObject(consumer->getParameters()),
consumer_(consumer),
isProcessing_(false),
mainThread_(*ThreadWrapper::CreateThread(Pipeliner::mainThreadRoutine, this)),
isPipelining_(false),
isPipelinePaused_(false),
isBuffering_(false),
pipelineIntervalMs_(0),
pipelineThread_(*ThreadWrapper::CreateThread(Pipeliner::pipelineThreadRoutin, this)),
pipelineTimer_(*EventWrapper::Create()),
pipelinerPauseEvent_(*EventWrapper::Create()),
deltaSegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumDelta)),
keySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumKey)),
rtxFreqMeterId_(NdnRtcUtils::setupFrequencyMeter())
{
    initialize();
}

ndnrtc::new_api::Pipeliner::~Pipeliner()
{
    
}

//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::Pipeliner::start()
{
    bufferEventsMask_ = ndnrtc::FrameBuffer::Event::StateChanged;
    isProcessing_ = true;
    
    unsigned int tid;
    mainThread_.Start(tid);
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::stop()
{
    if (isPipelining_)
        stopChasePipeliner();
    
    mainThread_.SetNotAlive();
    isProcessing_ = false;
    mainThread_.Stop();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - static

//******************************************************************************
#pragma mark - private
bool
ndnrtc::new_api::Pipeliner::processEvents()
{
    ndnrtc::new_api::FrameBuffer::Event
    event = frameBuffer_->waitForEvents(bufferEventsMask_);
    
    LogTrace(consumer_->getLogFile()) << "event "
        << FrameBuffer::Event::toString(event) << endl;
    
    switch (event.type_) {
        case FrameBuffer::Event::Error : {
            LogError(consumer_->getLogFile()) << "error on buffer events";
            isProcessing_ = false;
        }
            break;
        default:
        {
            if (frameBuffer_->getState() == FrameBuffer::Valid)
                handleValidState(event);
            else
                handleInvalidState(event);
        }
            break;
    }
    
    return isProcessing_;
}

int
ndnrtc::new_api::Pipeliner::handleInvalidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    LogTrace(consumer_->getLogFile()) << "handle invalid buf state" << endl;
    
    unsigned int activeSlotsNum = frameBuffer_->getActiveSlotsNum();
    
    if (activeSlotsNum == 0)
    {
        shared_ptr<Interest>
        rightmost = getInterestForRightMost(getInterestLifetime());
        
        express(*rightmost, getInterestLifetime());
        bufferEventsMask_ = FrameBuffer::Event::FirstSegment |
                            FrameBuffer::Event::Timeout;
    }
    else
        handleChase(event);
    
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::handleChase(const FrameBuffer::Event &event)
{
    if (event.type_ == FrameBuffer::Event::FirstSegment)
    {
        updateSegnumEstimation(event.slot_->getNamespace(),
                               event.slot_->getSegmentsNumber());
        
        int64_t lifetime = consumer_->getParameters().interestTimeout*1000;
        requestMissing(event.slot_, lifetime, lifetime);
    }
    
    unsigned int activeSlotsNum = frameBuffer_->getActiveSlotsNum();
    
    if (activeSlotsNum == 1)
        initialDataArrived(event.slot_);
    
    if (activeSlotsNum > 1)
    {
        if (isBuffering_)
            handleBuffering(event);
        else
            chaseDataArrived(event);
    }
    
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner::initialDataArrived
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>& slot)
{
    LogTrace(consumer_->getLogFile()) << "first data arrived";
    
    // request next key frame
    requestKeyFrame(slot->getPairedFrameNumber()+1);
    keyFrameSeqNo_++;
    
    double producerRate = (slot->getPacketRate())?slot->getPacketRate():params_.producerRate;
    bufferEstimator_->setProducerRate(producerRate);
    startChasePipeliner(slot->getSequentialNumber()+1, producerRate/2);
    
    bufferEventsMask_ = FrameBuffer::Event::FirstSegment | FrameBuffer::Event::Ready |
                        FrameBuffer::Event::Timeout | FrameBuffer::Event::StateChanged;
}

void
ndnrtc::new_api::Pipeliner::chaseDataArrived(const FrameBuffer::Event& event)
{
    switch (event.type_) {
        case FrameBuffer::Event::FirstSegment:
        {
            LogTrace(consumer_->getLogFile()) << "chaser track " << endl;
            chaseEstimation_->trackArrival();
        } // fall through
        default:
        {
            pipelineIntervalMs_ = chaseEstimation_->getArrivalEstimation();
#if 0
            if (event.slot_->getRecentSegment()->isOriginal())
            {
                LogTrace(consumer_->getLogFile()) << "chased producer succesfully" << endl;
                
                frameBuffer_->freeSlotsLessThan(event.slot_->getPrefix());
            }
#endif
            int bufferSize = frameBuffer_->getEstimatedBufferSize();
            assert(bufferSize >= 0);
            
            LogTrace(consumer_->getLogFile())
            << "buffer size " << bufferSize << endl;
            
            if (chaseEstimation_->isArrivalStable())
            {
                LogTrace(consumer_->getLogFile()) << "[****] arrival stable -> buffering. buffer size " << bufferSize << endl;
                
                stopChasePipeliner();
                frameBuffer_->setTargetSize(bufferEstimator_->getTargetSize());
                isBuffering_ = true;
            }
            else
            {
                if (bufferSize >
                    frameBuffer_->getTargetSize())
                {
                    LogTrace(consumer_->getLogFile())
                    << "buffer is too large - recycle old frames" << endl;
                    
                    setChasePipelinerPaused(true);
                    frameBuffer_->recycleOldSlots();
                    
                    bufferSize = frameBuffer_->getEstimatedBufferSize();
                    
                    LogTrace(consumer_->getLogFile())
                    << "new buffer size " << bufferSize << endl;
                }
                else
                {
                    if (isPipelinePaused_)
                    {
                        LogTrace(consumer_->getLogFile()) << "resuming chase pipeliner" << endl;
                        setChasePipelinerPaused(false);
                    }
                }
            }
        }
            break;
    }

}

void
ndnrtc::new_api::Pipeliner::handleBuffering(const FrameBuffer::Event& event)
{
    int bufferSize = frameBuffer_->getPlayableBufferSize();
    
    if (bufferSize >= frameBuffer_->getTargetSize())
    {
        LogTrace(consumer_->getLogFile())
        << "[*****] switch to valid state. playable size " << bufferSize << endl;
        
        frameBuffer_->setState(FrameBuffer::Valid);
        bufferEventsMask_ |= FrameBuffer::Event::Playout;
    }
    else
    {
        keepBuffer();
        
        LogTrace(consumer_->getLogFile())
        << "[*****] buffering. playable size " << bufferSize << endl;
        frameBuffer_->dump();
    }
}

int
ndnrtc::new_api::Pipeliner::handleValidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    int bufferSize = frameBuffer_->getEstimatedBufferSize();
    
    LogTrace(consumer_->getLogFile())
    << "handle valid buf state. "
    << "buffer size " << bufferSize
    << " target " << frameBuffer_->getTargetSize() << endl;
    
    switch (event.type_) {
        case FrameBuffer::Event::FirstSegment:
        {
            requestMissing(event.slot_, getInterestLifetime(event.slot_->getNamespace()), event.slot_->getPlaybackDeadline());
        }
            break;
            
        case FrameBuffer::Event::Timeout:
        {
            handleTimeout(event);
        }
            break;
        case FrameBuffer::Event::Ready:
        {
        }
            break;
        
        case FrameBuffer::Event::Playout:
        {
            // check, whether we should ask for a key frame
            if (event.slot_->getNamespace() == FrameBuffer::Slot::Key)
            {
                LogTrace(consumer_->getLogFile())
                << "key locked. request one more" << endl;
                
                requestKeyFrame(keyFrameSeqNo_);
                keyFrameSeqNo_++;
            }
        }
            break;
        default:
            break;
    }
    
    keepBuffer();
    
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner::handleTimeout(const FrameBuffer::Event &event)
{
    LogTrace(consumer_->getLogFile())
    << "timeout " << event.slot_->getPrefix()
    << " deadline " << event.slot_->getPlaybackDeadline() << endl;

    requestMissing(event.slot_,
                   getInterestLifetime(event.slot_->getNamespace()),
                   event.slot_->getPlaybackDeadline(), true);
}

int
ndnrtc::new_api::Pipeliner::initialize()
{
    params_ = consumer_->getParameters();
    frameBuffer_ = consumer_->getFrameBuffer();
    chaseEstimation_ = consumer_->getChaseEstimation();
    bufferEstimator_ = consumer_->getBufferEstimator();
    ndnAssembler_ = consumer_->getPacketAssembler();
    
    shared_ptr<string>
    streamPrefixString = NdnRtcNamespace::getStreamPrefix(params_);
    
    shared_ptr<string>
    deltaPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    shared_ptr<string>
    keyPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_, true);
    
    if (!streamPrefixString.get() || !deltaPrefixString.get() ||
        !keyPrefixString.get())
        return notifyError(RESULT_ERR, "producer frames prefixes \
                           were not provided");

    streamPrefix_ = Name(streamPrefixString->c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString->c_str());
    keyFramesPrefix_ = Name(keyPrefixString->c_str());
    
    return RESULT_OK;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner::getDefaultInterest(const ndn::Name &prefix, int64_t timeoutMs)
{
    shared_ptr<Interest> interest(new Interest(prefix, (timeoutMs == 0)?consumer_->getParameters().interestTimeout*1000:timeoutMs));
    interest->setMustBeFresh(true);
    
    return interest;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner::getInterestForRightMost(int64_t timeoutMs,
                                                    bool isKeyNamespace,
                                                    PacketNumber exclude)
{
    Name prefix = isKeyNamespace?keyFramesPrefix_:deltaFramesPrefix_;
    shared_ptr<Interest> rightmost = getDefaultInterest(prefix, timeoutMs);
    
    rightmost->setChildSelector(1);
    rightmost->setMinSuffixComponents(2);
    
    if (exclude >= 0)
    {
        rightmost->getExclude().appendAny();
        rightmost->getExclude().appendComponent(NdnRtcUtils::componentFromInt(exclude));
    }
    
    return rightmost;
}

void
ndnrtc::new_api::Pipeliner::updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                                   int nSegments)
{
    int estimatorId = (frameNs == FrameBuffer::Slot::Key)?
                        keySegnumEstimatorId_:deltaSegnumEstimatorId_;

    NdnRtcUtils::meanEstimatorNewValue(estimatorId, (double)nSegments);
}

void
ndnrtc::new_api::Pipeliner::requestKeyFrame(PacketNumber keyFrameNo)
{
    keyFrameSeqNo_ = keyFrameNo;

    prefetchFrame(keyFramesPrefix_,
                  keyFrameSeqNo_,
                  ceil(NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_))-1);
}

void
ndnrtc::new_api::Pipeliner::requestDeltaFrame(PacketNumber deltaFrameNo)
{
    deltaFrameSeqNo_ = deltaFrameNo;
    
    prefetchFrame(deltaFramesPrefix_,
                  deltaFrameSeqNo_,
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_))-1);
}

void
ndnrtc::new_api::Pipeliner::expressRange(Interest& interest,
                                         SegmentNumber startNo,
                                         SegmentNumber endNo, int64_t priority)
{
    Name prefix = interest.getName();
    
    LogTrace(consumer_->getLogFile()) << "express range "
    << interest.getName() << "/[" << startNo << ", " << endNo << "]"<< endl;
    
    std::vector<shared_ptr<Interest>> segmentInterests;
    frameBuffer_->interestRangeIssued(interest, startNo, endNo, segmentInterests);
    
    if (segmentInterests.size())
    {
        std::vector<shared_ptr<Interest>>::iterator it;
        for (it = segmentInterests.begin(); it != segmentInterests.end(); ++it)
        {
            shared_ptr<Interest> interestPtr = *it;
            
            consumer_->getInterestQueue()->enqueueInterest(*interestPtr,
                                                               Priority::fromArrivalDelay(priority),
                                                               ndnAssembler_->getOnDataHandler(),
                                                               ndnAssembler_->getOnTimeoutHandler());
        }
    }
}

void
ndnrtc::new_api::Pipeliner::express(Interest &interest, int64_t priority)
{
    LogTrace(consumer_->getLogFile()) << "enqueue " << interest.getName() << endl;
    
    frameBuffer_->interestIssued(interest);
    consumer_->getInterestQueue()->enqueueInterest(interest,
                                                       Priority::fromArrivalDelay(priority),
                                                       ndnAssembler_->getOnDataHandler(),
                                                       ndnAssembler_->getOnTimeoutHandler());
}

void
ndnrtc::new_api::Pipeliner::startChasePipeliner(PacketNumber startPacketNo,
                                                 int64_t intervalMs)
{
    LogTrace(consumer_->getLogFile()) << "start pipeline from "
        << startPacketNo << " interval = " << intervalMs << "ms" << endl;
    
    deltaFrameSeqNo_ = startPacketNo;
    pipelineIntervalMs_ = intervalMs;
    isPipelining_ = true;
    
    unsigned int tid;
    pipelineThread_.Start(tid);
}

void
ndnrtc::new_api::Pipeliner::setChasePipelinerPaused(bool isPaused)
{
    isPipelinePaused_ = isPaused;

    if (!isPaused)
        pipelinerPauseEvent_.Set();
}

void
ndnrtc::new_api::Pipeliner::stopChasePipeliner()
{
    isPipelining_ = false;
    isPipelinePaused_ = false;
    pipelineThread_.SetNotAlive();
    pipelinerPauseEvent_.Set();
    pipelineThread_.Stop();
}

bool
ndnrtc::new_api::Pipeliner::processPipeline()
{
    if (isPipelinePaused_)
    {
        LogTrace(consumer_->getLogFile()) << "pipeliner paused" << endl;
        pipelinerPauseEvent_.Wait(WEBRTC_EVENT_INFINITE);
        LogTrace(consumer_->getLogFile()) << "pipeliner woke up" << endl;
    }
    
    if (isPipelining_)
    {
        if (frameBuffer_->getEstimatedBufferSize() < frameBuffer_->getTargetSize())
        {
            requestDeltaFrame(deltaFrameSeqNo_);
            deltaFrameSeqNo_++;
        }
        
        pipelineTimer_.StartTimer(false, pipelineIntervalMs_);
        pipelineTimer_.Wait(pipelineIntervalMs_);
    }
    
    return isPipelining_;
}

void
ndnrtc::new_api::Pipeliner::requestMissing
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot,
 int64_t lifetime, int64_t priority, bool wasTimedOut)
{
    Name prefix = (slot->getNamespace() == FrameBuffer::Slot::Key)?
                    Name(keyFramesPrefix_):Name(deltaFramesPrefix_);
    prefix.append(NdnRtcUtils::componentFromInt(slot->getSequentialNumber()));
    
    // synchronize with buffer
    frameBuffer_->synchronizeAcquire();
    
    vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>
    missingSegments = slot->getMissingSegments();
    
    vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
    for (it = missingSegments.begin(); it != missingSegments.end(); ++it)
    {
        shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> segment = *it;
        
        assert(segment->getNumber() >= 0);
        
        shared_ptr<Interest> segmentInterest = getDefaultInterest(segment->getPrefix());
        segmentInterest->setInterestLifetimeMilliseconds(lifetime);
        
        LogTrace(consumer_->getLogFile()) << "to enqueue missing "
        << segmentInterest->getName() << endl;
        
        express(*segmentInterest, priority);
        
        if (wasTimedOut)
        {
            NdnRtcUtils::frequencyMeterTick(rtxFreqMeterId_);
            rtxNum_++;
        }
    }
    
    frameBuffer_->synchronizeRelease();
}

int64_t
ndnrtc::new_api::Pipeliner::getInterestLifetime(FrameBuffer::Slot::Namespace nspc)
{
    int64_t interestLifetime = 0;
    
    if (nspc == FrameBuffer::Slot::Key ||
        frameBuffer_->getState() == FrameBuffer::Invalid)
    {
        interestLifetime = params_.interestTimeout*1000;
    }
    else
    {
        interestLifetime = ceil(RttEstimation::sharedInstance().getCurrentEstimation());
    }
    
    return interestLifetime;
}

void
ndnrtc::new_api::Pipeliner::prefetchFrame(const ndn::Name &basePrefix,
                                          PacketNumber packetNo,
                                          int prefetchSize)
{
    Name packetPrefix(basePrefix);
    packetPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
    
    shared_ptr<Interest> frameInterest = getDefaultInterest(packetPrefix);
    expressRange(*frameInterest, 0, prefetchSize);
}

void
ndnrtc::new_api::Pipeliner::keepBuffer(bool useEstimatedSize)
{
    int bufferSize = (useEstimatedSize)?frameBuffer_->getEstimatedBufferSize():
    frameBuffer_->getPlayableBufferSize();
    
    while (bufferSize < frameBuffer_->getTargetSize())
    {
        LogTrace(consumer_->getLogFile())
        << "filling up the buffer with " << deltaFrameSeqNo_ << endl;
        
        requestDeltaFrame(deltaFrameSeqNo_);
        deltaFrameSeqNo_++;
        
        bufferSize = (useEstimatedSize)?frameBuffer_->getEstimatedBufferSize():
        frameBuffer_->getPlayableBufferSize();
    }
}

void
ndnrtc::new_api::Pipeliner::resetData()
{
    rtxNum_ = 0;
    rtxFreqMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    
}
