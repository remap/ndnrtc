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
#include "playout.h"
#include "params.h"

using namespace boost;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;

const double Pipeliner::SegmentsAvgNumDelta = 8.;
const double Pipeliner::SegmentsAvgNumKey = 25.;
const double Pipeliner::ParitySegmentsAvgNumDelta = 2.;
const double Pipeliner::ParitySegmentsAvgNumKey = 5.;
const int64_t Pipeliner::MaxInterruptionDelay = 1200;
const int64_t Pipeliner::MinInterestLifetime = 250;
const int Pipeliner::MaxRetryNum = 3;
const int Pipeliner::ChaserRateCoefficient = 4;
const int Pipeliner::FullBufferRecycle = 1;

#define CHASER_CHUNK_LVL 0.4
#define CHASER_CHECK_LVL (1-CHASER_CHUNK_LVL)

//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::Pipeliner::Pipeliner(const shared_ptr<Consumer> &consumer):
state_(StateInactive),
consumer_(consumer.get()),
mainThread_(*ThreadWrapper::CreateThread(Pipeliner::mainThreadRoutine, this, kHighPriority, "pipeliner-main")),
isPipelining_(false),
isPipelinePaused_(false),
pipelineIntervalMs_(0),
pipelineThread_(*ThreadWrapper::CreateThread(Pipeliner::pipelineThreadRoutin, this, kHighPriority, "pipeliner-chaser")),
pipelineTimer_(*EventWrapper::Create()),
pipelinerPauseEvent_(*EventWrapper::Create()),
deltaSegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumDelta)),
keySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumKey)),
deltaParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, ParitySegmentsAvgNumDelta)),
keyParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, ParitySegmentsAvgNumKey)),
rtxFreqMeterId_(NdnRtcUtils::setupFrequencyMeter()),
exclusionFilter_(-1),
useKeyNamespace_(true),
streamSwitchSync_(*CriticalSectionWrapper::CreateCriticalSection()),
streamId_(0)
{
    initialize();
}

ndnrtc::new_api::Pipeliner::~Pipeliner()
{
    mainThread_.~ThreadWrapper();
    pipelineThread_.~ThreadWrapper();
    pipelineTimer_.~EventWrapper();
    pipelinerPauseEvent_.~EventWrapper();
}

//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::Pipeliner::start()
{
    switchToState(StateChasing);
    bufferEventsMask_ = StartupEventsMask;
    stat_.nRebuffer_ = 0;
    reconnectNum_ = 0;
    deltaFrameSeqNo_ = -1;
    
    unsigned int tid;
    mainThread_.Start(tid);
    
    LogInfoC << "started" << std::endl;
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::stop()
{
    if (state_ == StateChasing)
        stopChasePipeliner();
    
    switchToState(StateInactive);
    frameBuffer_->release();
    mainThread_.SetNotAlive();
    mainThread_.Stop();
    
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner::triggerRebuffering()
{
    rebuffer();
}

void
ndnrtc::new_api::Pipeliner::threadSwitched()
{
    CriticalSectionScoped scopedCs(&streamSwitchSync_);

    std::string
    threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
    
    std::string
    deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
    
    std::string
    keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, true);
    
    threadPrefix_ = Name(threadPrefixString.c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
    keyFramesPrefix_ = Name(keyPrefixString.c_str());
    
    keyFrameSeqNo_ += 1;
    deltaFrameSeqNo_ += ((VideoThreadParams*)consumer_->getCurrentThreadParameters())->coderParams_.gop_;
}

PipelinerStatistics
ndnrtc::new_api::Pipeliner::getStatistics()
{
    stat_.avgSegNum_ = NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_);
    stat_.avgSegNumKey_ = NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_);
    stat_.avgSegNumParity_ = NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_);
    stat_.avgSegNumParityKey_ = NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_);
    stat_.rtxFreq_ = NdnRtcUtils::currentFrequencyMeterValue(rtxFreqMeterId_);
    
    return stat_;
}

//******************************************************************************
#pragma mark - static

//******************************************************************************
#pragma mark - private
bool
ndnrtc::new_api::Pipeliner::processEvents()
{
    ndnrtc::new_api::FrameBuffer::Event
    event = frameBuffer_->waitForEvents(bufferEventsMask_, MaxInterruptionDelay);
    
    LogDebugC << "Event " << FrameBuffer::Event::toString(event) << " ["
    << (event.slot_.get()?event.slot_->dump():"") << "]"
    << std::endl;
    
    switch (event.type_) {
        case FrameBuffer::Event::Error : {
            
            LogErrorC << "error on buffer events" << std::endl;
            switchToState(StateInactive);
        }
            break;
        case FrameBuffer::Event::Empty:
        {
            LogWarnC << "no activity in the buffer for " << MaxInterruptionDelay
            << " milliseconds" << std::endl;
            
        } break;
        case FrameBuffer::Event::FirstSegment:
        {
            updateSegnumEstimation(event.slot_->getNamespace(),
                                   event.slot_->getSegmentsNumber(),
                                   false);
            updateSegnumEstimation(event.slot_->getNamespace(),
                                   event.slot_->getParitySegmentsNumber(),
                                   true);
        } // fall through
        default:
        {
            if (frameBuffer_->getState() == FrameBuffer::Valid)
                handleValidState(event);
            else
                handleInvalidState(event);
        }
            break;
    }
    
    recoveryCheck(event);
    
    return (state_ != StateInactive);
}

int
ndnrtc::new_api::Pipeliner::handleInvalidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    unsigned int activeSlotsNum = frameBuffer_->getActiveSlotsNum();
    
    if (activeSlotsNum == 0)
    {
        shared_ptr<Interest>
        rightmost = getInterestForRightMost(getInterestLifetime(consumer_->getParameters().interestLifetime_),
                                            false, exclusionFilter_);
        
        express(*rightmost, getInterestLifetime(consumer_->getParameters().interestLifetime_));
        bufferEventsMask_ = WaitForRightmostEventsMask;
        recoveryCheckpointTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    }
    else
        handleChase(event);
    
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::handleChase(const FrameBuffer::Event &event)
{
    unsigned int activeSlotsNum = frameBuffer_->getActiveSlotsNum();
    
    if (event.type_ == FrameBuffer::Event::FirstSegment)
        reconnectNum_ = 0;
    
    if (activeSlotsNum == 1) // we're expecting initial data
    {
        if (event.type_ != FrameBuffer::Event::Timeout)
            initialDataArrived(event.slot_);
    }
    else
    {
        switch (event.type_) {
            case FrameBuffer::Event::Timeout:
            {
                handleTimeout(event);
            }
                break;
                
            case FrameBuffer::Event::FirstSegment:
            {
                if (state_ == StateBuffering)
                    requestMissing(event.slot_,
                                   getInterestLifetime(event.slot_->getPlaybackDeadline(),
                                                   event.slot_->getNamespace()),
                               0);
            }
                break;
            default:
                break;
        }
        
        if (state_ == StateBuffering)
            handleBuffering(event);
        
        if (state_ == StateChasing)
            handleChasing(event);
    }
    
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner::initialDataArrived
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>& slot)
{
    // we need to check for exclusion filter explicitly
    // there could be a case when data stopped coming for some time and
    // consumer switched to chase mode and re-issued rightmost interest with
    // EF, however previously sent interests were not cancelled (and there is
    // no such option currently in API) and old data can still arrive
    if (exclusionFilter_ == -1  || exclusionFilter_ < slot->getSequentialNumber())
    {
        LogTraceC << "initial data: [" << slot->dump() << "]" << std::endl;
        
#warning ???
        double producerRate = (slot->getPacketRate()>0)?slot->getPacketRate():30.;
        startChasePipeliner(slot->getSequentialNumber()+1, 1000./(ChaserRateCoefficient*producerRate));
        
        bufferEventsMask_ = ChasingEventsMask;
    }
    else
        LogWarnC << "got old data after rebuffering" << std::endl;
}

void
ndnrtc::new_api::Pipeliner::handleChasing(const FrameBuffer::Event& event)
{
    int bufferSize = frameBuffer_->getEstimatedBufferSize();
    assert(bufferSize >= 0);
    
    if (event.type_ == FrameBuffer::Event::FirstSegment)
    {
        chaseEstimation_->trackArrival(event.slot_->getPacketRate());
        
        if (event.slot_->getPairedFrameNumber() > keyFrameSeqNo_)
            keyFrameSeqNo_ = event.slot_->getPairedFrameNumber()+1;
    }
    
    if (chaseEstimation_->isArrivalStable())
    {
        stopChasePipeliner();
        switchToState(StateBuffering);
        
        frameBuffer_->setTargetSize(bufferEstimator_->getTargetSize());
        frameBuffer_->recycleOldSlots();

        keyFrameSeqNo_++;
        requestNextKey(keyFrameSeqNo_);
        
        bufferEventsMask_ = BufferingEventsMask;
        playbackStartFrameNo_ = deltaFrameSeqNo_;
        consumer_->getPacketPlayout()->setStartPacketNo(deltaFrameSeqNo_);
        handleBuffering(event);
    }
    else if (bufferSize >= frameBuffer_->getTargetSize() &&
             frameBuffer_->getFetchedSlotsNum() >= CHASER_CHECK_LVL*(double)frameBuffer_->getTotalSlotsNum())
    {
        int framesForRecycle = frameBuffer_->getTotalSlotsNum()*CHASER_CHUNK_LVL;
        frameBuffer_->recycleOldSlots(framesForRecycle);
        
        LogTraceC << "no stabilization after "
        << bufferSize << "ms has fetched. recycled "
        << framesForRecycle << " frames" << std::endl;
        
        setChasePipelinerPaused(false);
    }
    
    if (bufferSize < frameBuffer_->getTargetSize() && isPipelinePaused_)
        setChasePipelinerPaused(false);
}

void
ndnrtc::new_api::Pipeliner::handleBuffering(const FrameBuffer::Event& event)
{
    int targetSize = frameBuffer_->getTargetSize();
    int estimatedSize = frameBuffer_->getEstimatedBufferSize();
    int playableSize = frameBuffer_->getPlayableBufferSize();
    
    int nRequested = keepBuffer();
    
    if (playableSize >= (targetSize - consumer_->getRttEstimation()->getCurrentEstimation()) ||
        (nRequested == 0 && playableSize >= 0.7*targetSize))
    {   
        switchToState(StateFetching);
        frameBuffer_->setState(FrameBuffer::Valid);
        bufferEventsMask_ = FetchingEventsMask;
        
        if (callback_)
            callback_->onBufferingEnded();
    }
}

int
ndnrtc::new_api::Pipeliner::handleValidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    int bufferSize = frameBuffer_->getEstimatedBufferSize();
    
    switch (event.type_) {
        case FrameBuffer::Event::FirstSegment:
        {
            requestMissing(event.slot_,
                           getInterestLifetime(event.slot_->getPlaybackDeadline(),
                                               event.slot_->getNamespace()),
                           event.slot_->getPlaybackDeadline());
        }
            break;
            
        case FrameBuffer::Event::Timeout:
        {
            handleTimeout(event);
        }
            break;
        case FrameBuffer::Event::Ready:
        {
            LogStatC << "ready\t" << event.slot_->dump() << std::endl;
        }
            break;
        case FrameBuffer::Event::NeedKey:
        {
            requestNextKey(keyFrameSeqNo_);
        }
            break;
        case FrameBuffer::Event::Playout:
        {
            // check if frame counters are outdated
            if (event.slot_->getNamespace() == FrameBuffer::Slot::Key)
            {
                if (deltaFrameSeqNo_ < event.slot_->getPairedFrameNumber())
                {
                    deltaFrameSeqNo_ = event.slot_->getPairedFrameNumber();
                    
                    LogDebugC
                    << "pipeliner pointer updated " << deltaFrameSeqNo_
                    << std::endl;
                }
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
    if (consumer_->getGeneralParameters().useRtx_)
        requestMissing(event.slot_,
                       getInterestLifetime(event.slot_->getPlaybackDeadline(),
                                           event.slot_->getNamespace(),
                                           true),
                       event.slot_->getPlaybackDeadline(), true);
}

int
ndnrtc::new_api::Pipeliner::initialize()
{
    frameBuffer_ = consumer_->getFrameBuffer().get();
    chaseEstimation_ = consumer_->getChaseEstimation().get();
    bufferEstimator_ = consumer_->getBufferEstimator().get();
    ndnAssembler_ = consumer_->getPacketAssembler();
    
    std::string
    threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
    
    std::string
    deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
    
    std::string
    keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, true);

    threadPrefix_ = Name(threadPrefixString.c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
    keyFramesPrefix_ = Name(keyPrefixString.c_str());
    
    return RESULT_OK;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner::getDefaultInterest(const ndn::Name &prefix, int64_t timeoutMs)
{
    shared_ptr<Interest> interest(new Interest(prefix, (timeoutMs == 0)?consumer_->getParameters().interestLifetime_:timeoutMs));
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
                                                   int nSegments, bool isParity)
{
    int estimatorId = 0;
    
    if (isParity)
    {
        estimatorId = (frameNs == FrameBuffer::Slot::Key)?
    keyParitySegnumEstimatorId_:deltaParitySegnumEstimatorId_;
    }
    else
    {
        estimatorId = (frameNs == FrameBuffer::Slot::Key)?
    keySegnumEstimatorId_:deltaSegnumEstimatorId_;
    }
    
    NdnRtcUtils::meanEstimatorNewValue(estimatorId, (double)nSegments);
}

void
ndnrtc::new_api::Pipeliner::requestNextKey(PacketNumber& keyFrameNo)
{
    // just ignore if key namespace is not used
    if (!useKeyNamespace_)
        return;
        
    LogTraceC << "request key " << keyFrameNo << std::endl;
    stat_.nRequested_++;
    stat_.nRequestedKey_++;
    
    prefetchFrame(keyFramesPrefix_,
                  keyFrameNo++,
                  ceil(NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_))-1,
                  ceil(NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_))-1,
                  FrameBuffer::Slot::Key);
}

void
ndnrtc::new_api::Pipeliner::requestNextDelta(PacketNumber& deltaFrameNo)
{
    stat_.nRequested_++;
    prefetchFrame(deltaFramesPrefix_,
                  deltaFrameNo++,
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_))-1,
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_))-1);
}

void
ndnrtc::new_api::Pipeliner::expressRange(Interest& interest,
                                         SegmentNumber startNo,
                                         SegmentNumber endNo,
                                         int64_t priority, bool isParity)
{
    Name prefix = interest.getName();
    
    std::vector<shared_ptr<Interest> > segmentInterests;
    frameBuffer_->interestRangeIssued(interest, startNo, endNo, segmentInterests, isParity);
    
    if (segmentInterests.size())
    {
        std::vector<shared_ptr<Interest> >::iterator it;
        for (it = segmentInterests.begin(); it != segmentInterests.end(); ++it)
        {
            shared_ptr<Interest> interestPtr = *it;
            
            stat_.nInterestSent_++;
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
    frameBuffer_->interestIssued(interest);
    
    stat_.nInterestSent_++;
    consumer_->getInterestQueue()->enqueueInterest(interest,
                                                       Priority::fromArrivalDelay(priority),
                                                       ndnAssembler_->getOnDataHandler(),
                                                       ndnAssembler_->getOnTimeoutHandler());
}

void
ndnrtc::new_api::Pipeliner::startChasePipeliner(PacketNumber startPacketNo,
                                                 int64_t intervalMs)
{
    assert(intervalMs > 0);
    
    LogTraceC
    << "chaser started from "
    << startPacketNo << " interval = " << intervalMs << " ms" << std::endl;
    
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
    
    LogTraceC << "chaser stopped" << std::endl;
}

bool
ndnrtc::new_api::Pipeliner::processPipeline()
{
    if (isPipelinePaused_)
    {
        LogTraceC << "chaser paused" << std::endl;
        
        pipelinerPauseEvent_.Wait(WEBRTC_EVENT_INFINITE);
        
        LogTraceC << "chaser resumed" << std::endl;
    }
    else
    {
        assert(state_ == StateChasing);
        
        prefetchFrame(deltaFramesPrefix_, deltaFrameSeqNo_++, 0, -1);
        pipelineTimer_.StartTimer(false, pipelineIntervalMs_);
        pipelineTimer_.Wait(pipelineIntervalMs_);
        
        if (frameBuffer_->getEstimatedBufferSize() >= frameBuffer_->getTargetSize())
        {
            setChasePipelinerPaused(true);
        }
    }
    
    return isPipelining_;
}

void
ndnrtc::new_api::Pipeliner::requestMissing
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot,
 int64_t lifetime, int64_t priority, bool wasTimedOut)
{
    // synchronize with buffer
    frameBuffer_->synchronizeAcquire();
    
    std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> >
    missingSegments = slot->getMissingSegments();
    
    if (missingSegments.size() == 0)
        LogTraceC << "no missing segments for " << slot->getPrefix() << std::endl;
    
    std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> >::iterator it;
    for (it = missingSegments.begin(); it != missingSegments.end(); ++it)
    {
        shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> segment = *it;

        shared_ptr<Interest> segmentInterest;
        
        if (!slot->isRightmost())
        {
            assert((segment->getNumber() >= 0));
            segmentInterest = getDefaultInterest(segment->getPrefix(), lifetime);
        }
        else
            segmentInterest = getInterestForRightMost(lifetime,
                                                      slot->getNamespace()==FrameBuffer::Slot::Key,
                                                      exclusionFilter_);
        
        express(*segmentInterest, priority);
        segmentInterest->setInterestLifetimeMilliseconds(lifetime);
        
        if (wasTimedOut)
        {
            slot->incremenrRtxNum();
            NdnRtcUtils::frequencyMeterTick(rtxFreqMeterId_);
            stat_.nRtx_++;
        }
    }
    
    frameBuffer_->synchronizeRelease();
}

int64_t
ndnrtc::new_api::Pipeliner::getInterestLifetime(int64_t playbackDeadline,
                                                FrameBuffer::Slot::Namespace nspc,
                                                bool rtx)
{
    int64_t interestLifetime = 0;
    
    if (playbackDeadline <= 0)
        playbackDeadline = consumer_->getParameters().interestLifetime_;
    
    if (rtx || nspc != FrameBuffer::Slot::Key)
    {
        int64_t halfBufferSize = frameBuffer_->getEstimatedBufferSize()/2;
        
        if (halfBufferSize <= 0)
            halfBufferSize = playbackDeadline;
        
        interestLifetime = std::min(playbackDeadline, halfBufferSize);
    }
    else
    { // only key frames
        int64_t playbackBufSize = frameBuffer_->getPlayableBufferSize();
        double gopInterval = ((VideoThreadParams*)consumer_->getCurrentThreadParameters())->coderParams_.gop_/frameBuffer_->getCurrentRate()*1000;

#warning temporary solution!
        interestLifetime = 300;//gopInterval-playbackBufSize;
        
        if (interestLifetime <= 0)
            interestLifetime = gopInterval;
    }
    
    assert(interestLifetime > 0);
    return interestLifetime;
}

void
ndnrtc::new_api::Pipeliner::prefetchFrame(const ndn::Name &basePrefix,
                                          PacketNumber packetNo,
                                          int prefetchSize, int parityPrefetchSize,
                                          FrameBuffer::Slot::Namespace nspc)
{
    streamSwitchSync_.Enter();
    Name packetPrefix(basePrefix);
    streamSwitchSync_.Leave();
    
    packetPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
    
    int64_t playbackDeadline = frameBuffer_->getEstimatedBufferSize();
    shared_ptr<Interest> frameInterest = getDefaultInterest(packetPrefix);
    
    frameInterest->setInterestLifetimeMilliseconds(getInterestLifetime(playbackDeadline, nspc));
    expressRange(*frameInterest,
                 0,
                 prefetchSize,
                 playbackDeadline,
                 false);
    
    if (consumer_->getGeneralParameters().useFec_)
    {
        expressRange(*frameInterest,
                     0,
                     parityPrefetchSize,
                     playbackDeadline,
                     true);
    }
}

int
ndnrtc::new_api::Pipeliner::keepBuffer(bool useEstimatedSize)
{
    int bufferSize = (useEstimatedSize)?frameBuffer_->getEstimatedBufferSize():
    frameBuffer_->getPlayableBufferSize();
    
    int nRequested = 0;
    while (bufferSize < frameBuffer_->getTargetSize())
    {
        requestNextDelta(deltaFrameSeqNo_);
        bufferSize = (useEstimatedSize)?frameBuffer_->getEstimatedBufferSize():
        frameBuffer_->getPlayableBufferSize();
        nRequested++;
    }
    
    return nRequested;
}

void
ndnrtc::new_api::Pipeliner::resetData()
{
    deltaSegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumDelta);
    keySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, SegmentsAvgNumKey);
    deltaParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, ParitySegmentsAvgNumDelta);
    keyParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, ParitySegmentsAvgNumKey);
    rtxFreqMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    
    reconnectNum_ = 0;
    recoveryCheckpointTimestamp_ = -1;
    isPipelinePaused_ = false;
    switchToState(StateInactive);
    stopChasePipeliner();
    keyFrameSeqNo_ = -1;
    exclusionFilter_ = -1;
    
    frameBuffer_->reset();
}

void
ndnrtc::new_api::Pipeliner::recoveryCheck
(const ndnrtc::new_api::FrameBuffer::Event& event)
{
    if (event.type_ &
        (FrameBuffer::Event::FirstSegment | FrameBuffer::Event::Ready))
    {
        recoveryCheckpointTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    }
    
    if (recoveryCheckpointTimestamp_ > 0 &&
        NdnRtcUtils::millisecondTimestamp() - recoveryCheckpointTimestamp_ > MaxInterruptionDelay)
    {
        rebuffer();
    }
}

void
ndnrtc::new_api::Pipeliner::rebuffer()
{
    int nReconnects = reconnectNum_+1;
    stat_.nRebuffer_++;
    
    resetData();
    
    bufferEventsMask_ = StartupEventsMask;
    switchToState(StateChasing);
    
    chaseEstimation_->reset(state_ == StateFetching);
    reconnectNum_ = nReconnects;
    
    if (reconnectNum_ >= MaxRetryNum || deltaFrameSeqNo_ == -1)
    {
        exclusionFilter_ = -1;
    }
    else
        exclusionFilter_ = deltaFrameSeqNo_+1;
    
    if (callback_)
        callback_->onRebufferingOccurred();
    
    LogWarnC
    << "No data for the last " << MaxInterruptionDelay
    << " ms. Rebuffering " << stat_.nRebuffer_
    << " reconnect " << reconnectNum_
    << " exclusion " << exclusionFilter_
    << std::endl;
}
