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

const double PipelinerBase::SegmentsAvgNumDelta = 8.;
const double PipelinerBase::SegmentsAvgNumKey = 25.;
const double PipelinerBase::ParitySegmentsAvgNumDelta = 2.;
const double PipelinerBase::ParitySegmentsAvgNumKey = 5.;

const int64_t Pipeliner::MaxInterruptionDelay = 1200;
const int64_t Pipeliner::MinInterestLifetime = 250;
const int Pipeliner::MaxRetryNum = 3;
const int Pipeliner::ChaserRateCoefficient = 4;
const int Pipeliner::FullBufferRecycle = 1;

const int Pipeliner2::DefaultWindow = 8;
const int Pipeliner2::DefaultMinWindow = 2;

const FrameSegmentsInfo PipelinerBase::DefaultSegmentsInfo = {
    PipelinerBase::SegmentsAvgNumDelta,
    PipelinerBase::ParitySegmentsAvgNumDelta,
    PipelinerBase::SegmentsAvgNumKey,
    PipelinerBase::ParitySegmentsAvgNumKey
};

#define CHASER_CHUNK_LVL 0.4
#define CHASER_CHECK_LVL (1-CHASER_CHUNK_LVL)

//******************************************************************************
//******************************************************************************
#pragma mark - public
ndnrtc::new_api::PipelinerBase::PipelinerBase(const boost::shared_ptr<Consumer>& consumer,
                                              const FrameSegmentsInfo& frameSegmentsInfo):
callback_(nullptr),
state_(StateInactive),
consumer_(consumer.get()),
frameSegmentsInfo_(frameSegmentsInfo),
deltaSegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.deltaAvgSegNum_)),
keySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.keyAvgSegNum_)),
deltaParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.deltaAvgParitySegNum_)),
keyParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.keyAvgParitySegNum_)),
rtxFreqMeterId_(NdnRtcUtils::setupFrequencyMeter()),
useKeyNamespace_(true),
streamId_(0),
streamSwitchSync_(*CriticalSectionWrapper::CreateCriticalSection()),
frameBuffer_(consumer_->getFrameBuffer().get())
{
    switchToState(StateInactive);
}

ndnrtc::new_api::PipelinerBase::~PipelinerBase()
{
}

int
ndnrtc::new_api::PipelinerBase::initialize()
{
    ndnAssembler_ = consumer_->getPacketAssembler();
    
    std::string
    threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
    
    std::string
    deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
    
    // NOTE: keyPrefixString might be the same as deltaPrefixString if
    // key namespace is not used; this is needed for audio pipeliner
    std::string
    keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, useKeyNamespace_);
    
    threadPrefix_ = Name(threadPrefixString.c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
    keyFramesPrefix_ = Name(keyPrefixString.c_str());
    
    return RESULT_OK;
}

PipelinerStatistics
PipelinerBase::getStatistics()
{
    stat_.avgSegNum_ = NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_);
    stat_.avgSegNumKey_ = NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_);
    stat_.avgSegNumParity_ = NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_);
    stat_.avgSegNumParityKey_ = NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_);
    
    return stat_;
}

void
ndnrtc::new_api::PipelinerBase::triggerRebuffering()
{
    rebuffer();
}

void
ndnrtc::new_api::PipelinerBase::threadSwitched()
{
    CriticalSectionScoped scopedCs(&streamSwitchSync_);
    
    LogTraceC << "thread switched. rebuffer " << std::endl;
    
    std::string
    threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
    
    std::string
    deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
    
    std::string
    keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, useKeyNamespace_);
    
    threadPrefix_ = Name(threadPrefixString.c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
    keyFramesPrefix_ = Name(keyPrefixString.c_str());
    
    // for now, just rebuffer
    rebuffer();
}

#pragma mark - protected
void
ndnrtc::new_api::PipelinerBase::updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
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
ndnrtc::new_api::PipelinerBase::requestNextKey(PacketNumber& keyFrameNo)
{
    // just ignore if key namespace is not used
//    if (!useKeyNamespace_)
//        return;
    
    LogTraceC << "request key " << keyFrameNo << std::endl;
    stat_.nRequested_++;
    stat_.nRequestedKey_++;
    
    prefetchFrame(keyFramesPrefix_,
                  keyFrameNo++,
                  ceil(NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_)),
                  ceil(NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_)),
                  FrameBuffer::Slot::Key);
}

void
ndnrtc::new_api::PipelinerBase::requestNextDelta(PacketNumber& deltaFrameNo)
{
    stat_.nRequested_++;
    prefetchFrame(deltaFramesPrefix_,
                  deltaFrameNo++,
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_)),
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_)));
}

void
ndnrtc::new_api::PipelinerBase::expressRange(Interest& interest,
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
ndnrtc::new_api::PipelinerBase::express(Interest &interest, int64_t priority)
{
    frameBuffer_->interestIssued(interest);
    
    stat_.nInterestSent_++;
    consumer_->getInterestQueue()->enqueueInterest(interest,
                                                   Priority::fromArrivalDelay(priority),
                                                   ndnAssembler_->getOnDataHandler(),
                                                   ndnAssembler_->getOnTimeoutHandler());
}

void
ndnrtc::new_api::PipelinerBase::prefetchFrame(const ndn::Name &basePrefix,
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
                 prefetchSize-1,
                 playbackDeadline,
                 false);
    
    if (consumer_->getGeneralParameters().useFec_)
    {
        expressRange(*frameInterest,
                     0,
                     parityPrefetchSize-1,
                     playbackDeadline,
                     true);
    }
}

shared_ptr<Interest>
ndnrtc::new_api::PipelinerBase::getDefaultInterest(const ndn::Name &prefix, int64_t timeoutMs)
{
    shared_ptr<Interest> interest(new Interest(prefix, (timeoutMs == 0)?consumer_->getParameters().interestLifetime_:timeoutMs));
    interest->setMustBeFresh(true);
    
    return interest;
}

int64_t
ndnrtc::new_api::PipelinerBase::getInterestLifetime(int64_t playbackDeadline,
                                                FrameBuffer::Slot::Namespace nspc,
                                                bool rtx)
{
    int64_t interestLifetime = consumer_->getParameters().interestLifetime_;
    
    return interestLifetime;
    
    switch (state_) {
        case StateChasing: // fall through
        case StateWaitInitial: // fall through
        case StateAdjust:
        {
            interestLifetime = consumer_->getParameters().interestLifetime_;
        }
            break;
        case StateBuffering: // fall through
        case StateFetching: // fall through
        default:
        {
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
                
                interestLifetime = gopInterval-playbackBufSize;
                
                if (interestLifetime <= 0)
                    interestLifetime = gopInterval;
            }
        }
            break;
    }
    
    assert(interestLifetime > 0);
    return interestLifetime;
}

shared_ptr<Interest>
ndnrtc::new_api::PipelinerBase::getInterestForRightMost(int64_t timeoutMs,
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
ndnrtc::new_api::PipelinerBase::onRetransmissionNeeded(FrameBuffer::Slot* slot)
{
    if (consumer_->getGeneralParameters().useRtx_)
    {
        LogTraceC << "retransmission needed for " << slot->dump() << std::endl;
        
        std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>
        missingSegments = slot->getPendingSegments();
        
        if (missingSegments.size() == 0)
        {
            LogTraceC << "no missing segments for " << slot->getPrefix() << std::endl;
        }
        else
        {
            std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
            for (it = missingSegments.begin(); it != missingSegments.end(); ++it)
            {
                shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> segment = *it;
                shared_ptr<Interest> segmentInterest;

                segmentInterest = getDefaultInterest(segment->getPrefix());
                slot->markMissing(*segmentInterest);
                express(*segmentInterest, slot->getPlaybackDeadline());
            }

            slot->incremenrRtxNum();
            NdnRtcUtils::frequencyMeterTick(rtxFreqMeterId_);
            stat_.nRtx_++;
        }
    }
}

void
ndnrtc::new_api::PipelinerBase::requestMissing
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
                                                      slot->getNamespace()==FrameBuffer::Slot::Key);
        
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

void
ndnrtc::new_api::PipelinerBase::resetData()
{
    deltaSegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgSegNum_);
    keySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgSegNum_);
    deltaParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgParitySegNum_);
    keyParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgParitySegNum_);
    rtxFreqMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    
    recoveryCheckpointTimestamp_ = -1;
    switchToState(StateInactive);
    keyFrameSeqNo_ = -1;
    deltaFrameSeqNo_ = -1;
    
    frameBuffer_->reset();
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::Pipeliner::Pipeliner(const shared_ptr<Consumer> &consumer,
                                      const FrameSegmentsInfo& frameSegmentsInfo):
PipelinerBase(consumer, frameSegmentsInfo),
mainThread_(*ThreadWrapper::CreateThread(Pipeliner::mainThreadRoutine, this, kHighPriority, "pipeliner-main")),
isPipelining_(false),
isPipelinePaused_(false),
pipelineIntervalMs_(0),
pipelineThread_(*ThreadWrapper::CreateThread(Pipeliner::pipelineThreadRoutine, this, kHighPriority, "pipeliner-chaser")),
pipelineTimer_(*EventWrapper::Create()),
pipelinerPauseEvent_(*EventWrapper::Create()),
exclusionFilter_(-1)
{
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

PipelinerStatistics
ndnrtc::new_api::Pipeliner::getStatistics()
{
    PipelinerBase::getStatistics();
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
//        handleBuffering(event);
    }
    else if (bufferSize >= frameBuffer_->getTargetSize())// &&
//             frameBuffer_->getFetchedSlotsNum() >= CHASER_CHECK_LVL*(double)frameBuffer_->getTotalSlotsNum())
    {
        int framesForRecycle = 1;   //frameBuffer_->getTotalSlotsNum()*CHASER_CHUNK_LVL;
        frameBuffer_->recycleOldSlots(framesForRecycle);
        
        LogTraceC << "no stabilization after "
        << bufferSize << "ms has fetched. recycled "
        << framesForRecycle << " frames" << std::endl;
        
//        setChasePipelinerPaused(false);
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
    PipelinerBase::initialize();
    
    chaseEstimation_ = consumer_->getChaseEstimation().get();
    bufferEstimator_ = consumer_->getBufferEstimator().get();
    
    return RESULT_OK;
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
        
        prefetchFrame(deltaFramesPrefix_, deltaFrameSeqNo_++, 1, 0);
        pipelineTimer_.StartTimer(false, pipelineIntervalMs_);
        pipelineTimer_.Wait(pipelineIntervalMs_);
        
        if (frameBuffer_->getEstimatedBufferSize() >= frameBuffer_->getTargetSize())
        {
            setChasePipelinerPaused(true);
        }
    }
    
    return isPipelining_;
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
    PipelinerBase::resetData();
    
    reconnectNum_ = 0;
    isPipelinePaused_ = false;
    stopChasePipeliner();
    exclusionFilter_ = -1;
}

bool
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
        
        LogWarnC
        << "No data for the last " << MaxInterruptionDelay
        << " ms. Rebuffering " << stat_.nRebuffer_
        << " reconnect " << reconnectNum_
        << " exclusion " << exclusionFilter_
        << std::endl;
        
        return true;
    }
    
    return false;
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
}

//******************************************************************************
//******************************************************************************
PipelinerWindow::PipelinerWindow():
cs_(*CriticalSectionWrapper::CreateCriticalSection())
{
    
}

PipelinerWindow::~PipelinerWindow()
{
}

void
PipelinerWindow::init(unsigned int windowSize)
{
    framePool_.clear();
    dw_ = windowSize;
    w_ = (int)windowSize;
}

void
PipelinerWindow::dataArrived(PacketNumber packetNo)
{
    webrtc::CriticalSectionScoped scopedCs(&cs_);
    
    std::set<PacketNumber>::iterator it = framePool_.find(packetNo);
    
    if (it != framePool_.end())
    {
        w_++;
        framePool_.erase(it);
    }
}

bool
PipelinerWindow::canAskForData(PacketNumber packetNo)
{
    webrtc::CriticalSectionScoped scopedCs(&cs_);
    bool added = false;
    
    if (w_ > 0)
    {
        w_--;
        framePool_.insert(packetNo);
        lastAddedToPool_ = packetNo;
        added = true;
    }
    
    return added;
}

unsigned int
PipelinerWindow::getDefaultWindowSize()
{
    return dw_;
}

int
PipelinerWindow::getCurrentWindowSize()
{
    return w_;
}

bool
PipelinerWindow::changeWindow(int delta)
{
    if (dw_+delta > 0)
    {
        webrtc::CriticalSectionScoped scopedCs(&cs_);
        dw_ += delta;
        w_ += delta;
        
        return true;
    }
    
    return false;
}


//******************************************************************************
//******************************************************************************
Pipeliner2::Pipeliner2(const boost::shared_ptr<Consumer>& consumer,
                       const FrameSegmentsInfo& frameSegmentsInfo):
PipelinerBase(consumer, frameSegmentsInfo),
stabilityEstimator_(7, 4, 0.15, 0.7),
rttChangeEstimator_(7, 3, 0.12)
{
}

Pipeliner2::~Pipeliner2(){
    
}

int
Pipeliner2::start()
{
    stat_.nRebuffer_ = 0;
    deltaFrameSeqNo_ = -1;
    failedWindow_ = DefaultMinWindow;
    switchToState(StateWaitInitial);
    askForRightmostData();
}

int
Pipeliner2::stop(){
    switchToState(StateInactive);
    frameBuffer_->release();
    
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
Pipeliner2::onData(const boost::shared_ptr<const Interest>& interest,
                   const boost::shared_ptr<Data>& data)
{
    LogTraceC << "data " << data->getName() << std::endl;
    bool isKeyPrefix = NdnRtcNamespace::isPrefix(data->getName(), keyFramesPrefix_);
    
    switch (state_) {
        case StateWaitInitial:
        {
            // make sure we've got what is expected
//            if (NdnRtcNamespace::isKeyFramePrefix(data->getName()))
            if (isKeyPrefix)
            {
                LogTraceC << "received rightmost data "
                << data->getName() << std::endl;
                
                PrefixMetaInfo metaInfo;
                PrefixMetaInfo::extractMetadata(data->getName(), metaInfo);
                
                seedFrameNo_ = metaInfo.playbackNo_;
                askForInitialData(data);
            }
        }
            break;
        case StateChasing:
        {
            PrefixMetaInfo metaInfo;
            PrefixMetaInfo::extractMetadata(data->getName(), metaInfo);
            
            // make sure that we've got what is expected
            if (metaInfo.playbackNo_ > seedFrameNo_)
            {
                if (deltaFrameSeqNo_ < 0 &&
                    isKeyPrefix)
//                    NdnRtcNamespace::isKeyFramePrefix(data->getName()))
                {
                    deltaFrameSeqNo_ = metaInfo.pairedSequenceNo_;
                    timestamp_ = NdnRtcUtils::millisecondTimestamp();
                    
                    LogTraceC << "received initial data "
                    << data->getName()
                    << " start fetching delta from " << deltaFrameSeqNo_ << std::endl;
                }
            }
            else
            {
                LogTraceC << "got outdated data after rebuffering " << data->getName() << std::endl;
                break;
            }
        } // fall through
        case StateAdjust: // fall through
        case StateFetching:
        {
            askForSubsequentData(data);
        }
            break;
        default:
        {
            LogWarnC << "got data " << data->getName()
            << " in state " << toString(state_) << std::endl;
        }
            break;
    }
}

void
Pipeliner2::onTimeout(const boost::shared_ptr<const Interest>& interest)
{
    switch (state_) {
        case StateWaitInitial: // fall through
        {
            askForRightmostData();
        }
            break;
        case StateChasing:
        case StateFetching:
        {
            // do something with timeouts
        }
        default:
        {
            // ignore
            LogWarnC << "got timeout for " << interest->getName()
            << " in state " << toString(state_) << std::endl;
        }
            break;
    }
}

bool
Pipeliner2::recoveryCheck()
{
    int interruptionDelay = 500;
    
    if (recoveryCheckpointTimestamp_ > 0 &&
        NdnRtcUtils::millisecondTimestamp() - recoveryCheckpointTimestamp_ > interruptionDelay)
    {
        LogWarnC
        << "No data for the last " << interruptionDelay
        << " ms. Rebuffering " << stat_.nRebuffer_
        << " curent w " << window_.getCurrentWindowSize()
        << " default w " << window_.getDefaultWindowSize()
        << std::endl;
        
        rebuffer();
        
        return true;
    }
    
    return false;
}

//******************************************************************************
void
Pipeliner2::askForRightmostData()
{
    // issue rightmost
    Interest rightmostKeyInterest(keyFramesPrefix_, 1000);
    rightmostKeyInterest.setMustBeFresh(true);
    rightmostKeyInterest.setChildSelector(1);
    rightmostKeyInterest.setMinSuffixComponents(2);
    
    consumer_->getInterestQueue()->enqueueInterest(rightmostKeyInterest,
                                                   Priority::fromAbsolutePriority(0),
                                                   ndnAssembler_->getOnDataHandler(),
                                                   ndnAssembler_->getOnTimeoutHandler());
}

void
Pipeliner2::askForInitialData(const boost::shared_ptr<Data>& data)
{
    PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(data->getName());

    if (frameNo >= 0)
    {
        window_.init(DefaultWindow);
        frameBuffer_->setState(FrameBuffer::Valid);
        if (useKeyNamespace_)
        {
            keyFrameSeqNo_ = frameNo+1;
            requestNextKey(keyFrameSeqNo_);
        }
        else
        {
            deltaFrameSeqNo_ = frameNo+1;
            requestNextDelta(deltaFrameSeqNo_);
        }
        switchToState(StateChasing);
    }
    else
    {
        LogErrorC << "can't get key frame number from " << data->getName()
        << ". reset to chasing" << std::endl;
        
        frameBuffer_->reset();
        askForRightmostData();
    }
}

void
Pipeliner2::askForSubsequentData(const boost::shared_ptr<Data>& data)
{
    frameBuffer_->synchronizeAcquire();
    FrameBuffer::Event event = frameBuffer_->newData(*data);
    
    if (frameBuffer_->getPlayableBufferSize() > frameBuffer_->getTargetSize()/2.)
        if (consumer_->getGeneralParameters().useRtx_)
            frameBuffer_->setRetransmissionsEnabled(true);
    
    if (event.type_ == FrameBuffer::Event::Error)
    {
        frameBuffer_->synchronizeRelease();
        return;
    }
    
    bool needIncreaseWindow = false;
    bool needDecreaseWindow = false;
    bool isDeltaFrame = NdnRtcNamespace::isDeltaFramePrefix(data->getName());
    const int timeout = 1000;
    uint64_t currentTimestamp = NdnRtcUtils::millisecondTimestamp();
    bool isTimedOut = (currentTimestamp-timestamp_ >= timeout);
    
    recoveryCheckpointTimestamp_ = currentTimestamp;
    
    // normally, we track inter-arrival delay when we receive first segment of
    // a frame. however, Event::FirstSegment can be overriden by Event::Ready
    // if the frame we received consists of just 1 segment. that's why
    // besides FirstSegment events we have to care about Ready events as
    // well - for the frames that made up from 1 segment only
    bool isLegitimateForStabilityTracking =
        event.type_ == FrameBuffer::Event::FirstSegment ||
        (event.type_ == FrameBuffer::Event::Ready &&
         event.slot_->getSegmentsNumber() == 1);
    
    if (isLegitimateForStabilityTracking)
    {
        // update stability estimator
        stabilityEstimator_.trackInterArrival(frameBuffer_->getCurrentRate());

        // update window if it is a delta frame
        if (isDeltaFrame)
            window_.dataArrived(event.slot_->getSequentialNumber());
    }
    
    // if we're not in stable phase - flush RTT change estimator
    if (!stabilityEstimator_.isStable())
        rttChangeEstimator_.flush();
    
    if (isDeltaFrame)
    {
        rttChangeEstimator_.newRttValue(event.slot_->getRecentSegment()->getRoundTripDelayUsec()/1000.);
    }
    else // if KEY
        if (event.type_ == FrameBuffer::Event::Ready)
            requestNextKey(keyFrameSeqNo_);
    
    if (event.type_ == FrameBuffer::Event::FirstSegment)
        requestMissing(event.slot_,
                       getInterestLifetime(event.slot_->getPlaybackDeadline(),
                                           event.slot_->getNamespace()),
                       event.slot_->getPlaybackDeadline());
    
    switch (state_) {
        case StateChasing:
        {
            if (stabilityEstimator_.isStable())
            {
                switchToState(StateAdjust); // should fall through the next
                                            // case in this switch
            }
            else
            {
                if (isTimedOut)
                {
                    LogTraceC << "increase during chase" << std::endl;
                    
                    needIncreaseWindow = true;
                    timestamp_ = currentTimestamp;
                    waitForChange_ = false;
                }
                
                waitForStability_ = true;
                break; // break now
            }
        } // this case does not have an explicit break - as it only breaks in
          // certain conditions (check the code above)
            
        case StateAdjust:
        {
            if (rttChangeEstimator_.isStable())
            {
                if (waitForChange_)
                {
                    if (rttChangeEstimator_.hasChange())
                    {
                        LogTraceC << "RTT has changed. Waiting"
                        " for RTT stabilization" << std::endl;
                        
                        rttChangeEstimator_.flush();
                        waitForChange_ = false;
                        waitForStability_ = true;
                        timestamp_ = currentTimestamp;
                    }
                    else if (currentTimestamp-timestamp_ >= timeout)
                    {
                        LogTraceC << "timeout waiting for change" << std::endl;
                        
                        needDecreaseWindow = true;
                        timestamp_ = currentTimestamp;
                    }
                }
                else if (waitForStability_)
                {
                    LogTraceC << "decrease window" << std::endl;
                    
                    needDecreaseWindow = true;
                    waitForChange_ = true;
                    waitForStability_ = false;
                    timestamp_ = currentTimestamp;
                }
            }
            else
            {
                if (!stabilityEstimator_.isStable() &&
                    currentTimestamp-timestamp_ >= timeout)
                {
                    failedWindow_ = window_.getDefaultWindowSize();
                    
                    LogTraceC << "increase. failed window " << failedWindow_ << std::endl;
                    
                    needIncreaseWindow = true;
                    waitForStability_ = true;
                    waitForChange_ = false;
                    timestamp_ = currentTimestamp;
                }
            }
        }
            break;
            
        case StateFetching:
        {
            if (isTimedOut)
            {
                bool unstable = !stabilityEstimator_.isStable();
                bool lowBuffer = ((double)frameBuffer_->getPlayableBufferSize() / (double)frameBuffer_->getEstimatedBufferSize() < 0.5);
                // disable draining buffer check
                bool drainingBuffer = false; //(frameBuffer_->getEstimatedBufferSize() < frameBuffer_->getTargetSize());
                
                if (unstable || lowBuffer || drainingBuffer)
                {
                    LogWarnC
                    << "something wrong in fetching mode. unstable: " << unstable
                    << " draining buffer: " << drainingBuffer
                    << " low buffer: " << lowBuffer
                    << ". adjusting" << std::endl;
                    
                    switchToState(StateAdjust);
                    needIncreaseWindow = unstable || drainingBuffer;
                    waitForStability_ = true;
                    waitForChange_ = false;
                    timestamp_ = currentTimestamp;
                    
                    if (lowBuffer && !unstable)
                        failedWindow_ = DefaultMinWindow;
                }
            }
        }
            break;
            
        default:
            // nothing
            break;
    }
    
    if (needDecreaseWindow || needIncreaseWindow)
    {
        double frac = 0.5;
        int delta = 0;
        if (needIncreaseWindow)
        {
            if (state_ == StateChasing) // grow faster
                delta = window_.getDefaultWindowSize();
            else
                delta = int(ceil(frac*float(window_.getDefaultWindowSize())));
        }
        else if (needDecreaseWindow)
        {
            delta = -int(ceil(frac*float(window_.getDefaultWindowSize())));

            if (failedWindow_ >= delta+window_.getDefaultWindowSize())
            {
                LogTraceC << "trying to decrease lower than fail window "
                << failedWindow_ << std::endl;
                
                switchToState(StateFetching);
                timestamp_ = currentTimestamp;
                delta = 0;
            }
        }
        
        LogTraceC << "change window by " << delta << std::endl;
        
        window_.changeWindow(delta);
        
        LogTraceC << "current default window "
        << window_.getDefaultWindowSize() << std::endl;
    }
    
    // start playback whenever chasing phase is over and buffer has enough
    // size for playback
    if (state_ > StateChasing &&
        frameBuffer_->getPlayableBufferSize() >= frameBuffer_->getTargetSize())
        callback_->onBufferingEnded();
    
    while (window_.canAskForData(deltaFrameSeqNo_))
        requestNextDelta(deltaFrameSeqNo_);

    if (isLegitimateForStabilityTracking)
        LogStatC
        << "\trtt\t" << event.slot_->getRecentSegment()->getRoundTripDelayUsec()/1000.
        << "\trtt stable\t" << rttChangeEstimator_.isStable()
        << "\twin\t" << window_.getCurrentWindowSize()
        << "\tdwin\t" << window_.getDefaultWindowSize()
        << "\tdarr\t" << stabilityEstimator_.getLastDelta()
        << "\tstable\t" << stabilityEstimator_.isStable()
        << std::endl;
    
    frameBuffer_->synchronizeRelease();
}

void
Pipeliner2::resetData()
{
    PipelinerBase::resetData();
    
    timestamp_ = 0;
    failedWindow_ = DefaultMinWindow;
    seedFrameNo_ = -1;
    waitForChange_ = false;
    waitForStability_ = false;
    rttChangeEstimator_.flush();
    stabilityEstimator_.flush();
}

void
Pipeliner2::rebuffer()
{
    stat_.nRebuffer_++;
    resetData();
    switchToState(StateWaitInitial);
    askForRightmostData();
    
    if (callback_)
        callback_->onRebufferingOccurred();
}
