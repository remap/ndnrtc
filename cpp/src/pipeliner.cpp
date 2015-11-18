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
#include "consumer.h"
#include "interest-queue.h"

using namespace boost;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;
using namespace ndnlog;

const double Pipeliner2::SegmentsAvgNumDelta = 8.;
const double Pipeliner2::SegmentsAvgNumKey = 25.;
const double Pipeliner2::ParitySegmentsAvgNumDelta = 2.;
const double Pipeliner2::ParitySegmentsAvgNumKey = 5.;

const int Pipeliner2::DefaultWindow = 8;
const int Pipeliner2::DefaultMinWindow = 2;

const FrameSegmentsInfo Pipeliner2::DefaultSegmentsInfo = {
    Pipeliner2::SegmentsAvgNumDelta,
    Pipeliner2::ParitySegmentsAvgNumDelta,
    Pipeliner2::SegmentsAvgNumKey,
    Pipeliner2::ParitySegmentsAvgNumKey
};

#define CHASER_CHUNK_LVL 0.4
#define CHASER_CHECK_LVL (1-CHASER_CHUNK_LVL)

//******************************************************************************
//******************************************************************************
#pragma mark - public
ndnrtc::new_api::Pipeliner2::Pipeliner2(const boost::shared_ptr<Consumer>& consumer,
                                              const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
                                              const FrameSegmentsInfo& frameSegmentsInfo):
StatObject(statStorage),
callback_(nullptr),
state_(StateInactive),
startPhaseTimestamp_(NdnRtcUtils::millisecondTimestamp()),
consumer_(consumer.get()),
frameSegmentsInfo_(frameSegmentsInfo),
deltaSegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.deltaAvgSegNum_)),
keySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.keyAvgSegNum_)),
deltaParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.deltaAvgParitySegNum_)),
keyParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo.keyAvgParitySegNum_)),
rtxFreqMeterId_(NdnRtcUtils::setupFrequencyMeter()),
useKeyNamespace_(true),
streamId_(0),
frameBuffer_(consumer_->getFrameBuffer().get()),
recoveryCheckpointTimestamp_(0),
stabilityEstimator_(10, 4, 0.3, 0.7),
rttChangeEstimator_(7, 3, 0.12),
dataMeterId_(NdnRtcUtils::setupDataRateMeter(5)),
segmentFreqMeterId_(NdnRtcUtils::setupFrequencyMeter(10)),
exclusionPacket_(0),
waitForThreadTransition_(false)
{
    switchToState(StateInactive);
}

ndnrtc::new_api::Pipeliner2::~Pipeliner2()
{
}

int
ndnrtc::new_api::Pipeliner2::initialize()
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

int
Pipeliner2::start()
{
    deltaFrameSeqNo_ = -1;
    failedWindow_ = DefaultMinWindow;
    switchToState(StateWaitInitial);
    askForRightmostData();
    
    return RESULT_OK;
}

int
Pipeliner2::stop()
{
    switchToState(StateInactive);
    window_.reset();
    
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner2::triggerRebuffering()
{
    rebuffer();
}

bool
ndnrtc::new_api::Pipeliner2::threadSwitched()
{
    // if thread sycn list is available - use it
    if (deltaSyncList_.size() && keySyncList_.size())
    {
        // schedule thread transition when key frame will arrive
        waitForThreadTransition_ = true;
    }
    else // otherwise - rebuffer
    {
        std::string
        threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
        std::string
        deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
        std::string
        keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, useKeyNamespace_);
        
        threadPrefix_ = Name(threadPrefixString.c_str());
        deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
        keyFramesPrefix_ = Name(keyPrefixString.c_str());
        
        LogTraceC << "thread switched. rebuffer " << std::endl;
        rebuffer();
        
        return false;
    }
    
    return true;
}

#pragma mark - protected
void
ndnrtc::new_api::Pipeliner2::updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                                       int nSegments, bool isParity)
{
    Indicator statIndicator;
    int estimatorId = 0;
    
    if (isParity)
    {
        estimatorId = (frameNs == FrameBuffer::Slot::Key)?keyParitySegnumEstimatorId_:deltaParitySegnumEstimatorId_;
        statIndicator = (frameNs == FrameBuffer::Slot::Key)?Indicator::SegmentsKeyParityAvgNum : Indicator::SegmentsDeltaParityAvgNum;
    }
    else
    {
        estimatorId = (frameNs == FrameBuffer::Slot::Key)?keySegnumEstimatorId_:deltaSegnumEstimatorId_;
        statIndicator = (frameNs == FrameBuffer::Slot::Key)?Indicator::SegmentsKeyAvgNum : Indicator::SegmentsDeltaAvgNum;
    }
    
    NdnRtcUtils::meanEstimatorNewValue(estimatorId, (double)nSegments);
    (*statStorage_)[statIndicator] = nSegments;
}

void
ndnrtc::new_api::Pipeliner2::requestNextKey(PacketNumber& keyFrameNo)
{
    LogTraceC << "request key " << keyFrameNo << std::endl;
    (*statStorage_)[Indicator::RequestedNum]++;
    (*statStorage_)[Indicator::RequestedKeyNum]++;
    
    prefetchFrame(keyFramesPrefix_,
                  keyFrameNo++, // increment key frame counter
                  ceil(NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_)),
                  ceil(NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_)),
                  FrameBuffer::Slot::Key);
}

void
ndnrtc::new_api::Pipeliner2::requestNextDelta(PacketNumber& deltaFrameNo)
{
    (*statStorage_)[Indicator::RequestedNum]++;
    prefetchFrame(deltaFramesPrefix_,
                  deltaFrameNo++,
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_)),
                  ceil(NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_)));
}

void
ndnrtc::new_api::Pipeliner2::expressRange(Interest& interest,
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
            int segmentIdx = it - segmentInterests.begin();
            
            // lower priority for parity data
            int64_t pri = priority + segmentIdx + isParity*100;
            
            consumer_->getInterestQueue()->enqueueInterest(*interestPtr,
                                                           Priority::fromArrivalDelay(pri),
                                                           ndnAssembler_->getOnDataHandler(),
                                                           ndnAssembler_->getOnTimeoutHandler());
        }
    }
}

void
ndnrtc::new_api::Pipeliner2::express(Interest &interest, int64_t priority)
{
    frameBuffer_->interestIssued(interest);
    
    consumer_->getInterestQueue()->enqueueInterest(interest,
                                                   Priority::fromArrivalDelay(priority),
                                                   ndnAssembler_->getOnDataHandler(),
                                                   ndnAssembler_->getOnTimeoutHandler());
}

void
ndnrtc::new_api::Pipeliner2::prefetchFrame(const ndn::Name &basePrefix,
                                              PacketNumber packetNo,
                                              int prefetchSize, int parityPrefetchSize,
                                              FrameBuffer::Slot::Namespace nspc)
{
    Name packetPrefix(basePrefix);
    
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
ndnrtc::new_api::Pipeliner2::getDefaultInterest(const ndn::Name &prefix, int64_t timeoutMs)
{
    shared_ptr<Interest> interest(new Interest(prefix, (timeoutMs == 0)?consumer_->getParameters().interestLifetime_:timeoutMs));
    interest->setMustBeFresh(true);
    
    return interest;
}

int64_t
ndnrtc::new_api::Pipeliner2::getInterestLifetime(int64_t playbackDeadline,
                                                FrameBuffer::Slot::Namespace nspc,
                                                bool rtx)
{
//    StateChasing, StateWaitInitial 5000msec; StateAdjust,StateBuffering,StateFetching 2000msec
    int64_t interestLifetime = 0;

    switch (state_){
        case StateChasing: // fall through
        case StateWaitInitial:
        {
            interestLifetime = Consumer::MaxChasingTimeMs;
        }
        break;
        case StateAdjust: // fall through
        case StateBuffering: // fall through
        case StateFetching: // fall through
        default:
        {
            interestLifetime = consumer_->getParameters().interestLifetime_;
        }
        break;
    }
    
    assert(interestLifetime > 0);
    return interestLifetime;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner2::getInterestForRightMost(int64_t timeoutMs,
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
ndnrtc::new_api::Pipeliner2::onRetransmissionNeeded(FrameBuffer::Slot* slot)
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
            LogWarnC << "retransmit "
            << slot->dump()
            << " total " << missingSegments.size() << " interests"
            << std::endl;
            
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
            (*statStorage_)[Indicator::RtxFrequency] = NdnRtcUtils::currentFrequencyMeterValue(rtxFreqMeterId_);
            (*statStorage_)[Indicator::RtxNum]++;
            
            LogStatC << "rtx"
            << STAT_DIV << (*statStorage_)[Indicator::RtxNum] << std::endl;
        }
    }
}

void
ndnrtc::new_api::Pipeliner2::onKeyNeeded(PacketNumber seqNo)
{
    if (keyFrameSeqNo_ <= seqNo)
    {
        keyFrameSeqNo_ = seqNo;
        requestNextKey(keyFrameSeqNo_);
    }
}

void
ndnrtc::new_api::Pipeliner2::requestMissing
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot,
 int64_t lifetime, int64_t priority)
{
    // synchronize with buffer
    frameBuffer_->synchronizeAcquire();
    
    std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> >
    missingSegments = slot->getMissingSegments();
    
    if (missingSegments.size() == 0)
        LogTraceC << "no missing segments for " << slot->getPrefix() << std::endl;
    else
        LogDebugC << "request missing "
        << slot->getPrefix()
        << " total " << missingSegments.size() << " interests"
        << std::endl;
        
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
    }
    
    frameBuffer_->synchronizeRelease();
}

void
ndnrtc::new_api::Pipeliner2::resetData()
{
    deltaSegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgSegNum_);
    keySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgSegNum_);
    deltaParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgParitySegNum_);
    keyParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgParitySegNum_);
    rtxFreqMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    
    recoveryCheckpointTimestamp_ = 0;
    switchToState(StateInactive);
    keyFrameSeqNo_ = -1;
    deltaFrameSeqNo_ = -1;
    
    frameBuffer_->reset();
    
    timestamp_ = 0;
    failedWindow_ = DefaultMinWindow;
    seedFrameNo_ = -1;
    waitForChange_ = false;
    waitForStability_ = false;
    rttChangeEstimator_.flush();
    stabilityEstimator_.flush();
    window_.reset();
}

//******************************************************************************

void
Pipeliner2::onData(const boost::shared_ptr<const Interest>& interest,
                   const boost::shared_ptr<Data>& data)
{
    LogDebugC
    << "data " << data->getName() << " "
    << data->getContent().size() << " bytes" << std::endl;
    
    NdnRtcUtils::dataRateMeterMoreData(dataMeterId_, data->getDefaultWireEncoding().size());
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeterId_);
    recoveryCheckpointTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    
    (*statStorage_)[Indicator::SegmentsReceivedNum]++;
    statStorage_->updateIndicator(Indicator::InRateSegments, NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeterId_));
    (*statStorage_)[Indicator::InBitrateKbps] = NdnRtcUtils::currentDataRateMeterValue(dataMeterId_)*8./1000.;
    
    bool isKeyPrefix = NdnRtcNamespace::isPrefix(data->getName(), keyFramesPrefix_);
    PrefixMetaInfo metaInfo;
    PrefixMetaInfo::extractMetadata(data->getName(), metaInfo);
    
    switch (state_) {
        case StateWaitInitial:
        {
            // make sure we've got what we expected
            if (rightmostInterest_.getName() == interest->getName())
            {
                LogTraceC << "received rightmost data "
                << data->getName() << std::endl;
                
                seedFrameNo_ = metaInfo.playbackNo_;
                askForInitialData(data);
            }
        }
            break;
        case StateChasing:
        {
            // make sure that we've got what is expected
            if (metaInfo.playbackNo_ >= seedFrameNo_)
            {
                if (deltaFrameSeqNo_ < 0 &&
                    isKeyPrefix)
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
            updateThreadSyncList(metaInfo, isKeyPrefix);
            
            if (isKeyPrefix && waitForThreadTransition_)
                performThreadTransition();
            
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
    LogDebugC << "got timeout for " << interest->getName()
    << " in state " << toString(state_) << std::endl;
    
    (*statStorage_)[Indicator::TimeoutsNum]++;
    
    switch (state_) {
        case StateWaitInitial: // fall through
        {
            // make sure that we allow retransmission only when rightmost
            // interest is timed out, not interests from previous runs (in
            // case rebuffering happened)
            if (rightmostInterest_.getName() == interest->getName())
            {
                LogDebugC << "timeout "
                << interest->getName()
                << std::endl;
                
                askForRightmostData();
            }
        }
            break;
        case StateAdjust:
        case StateChasing:
        case StateFetching:
        {
            bool isKeyPrefix = NdnRtcNamespace::isPrefix(interest->getName(), keyFramesPrefix_);

            if (isKeyPrefix)
            {
                PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());

                if (packetNo == keyFrameSeqNo_)
                {
                    LogDebugC << "re-express "
                    << interest->getName()
                    << std::endl;
                    
                    consumer_->getInterestQueue()->enqueueInterest(*interest,
                                                                   Priority::fromAbsolutePriority(0),
                                                                   ndnAssembler_->getOnDataHandler(),
                                                                   ndnAssembler_->getOnTimeoutHandler());
                }
            }
            
        }
            break;
        default:
        {
            // ignore
        }
            break;
    }
}

void
Pipeliner2::onFrameDropped(PacketNumber sequenceNo,
                           PacketNumber playbackNo,
                           FrameBuffer::Slot::Namespace nspc)
{
    
    if (nspc == FrameBuffer::Slot::Delta)
        window_.dataArrived(sequenceNo);
    
    LogWarnC << "frame dropped (possibly lost) seq " << sequenceNo
    << " play " << playbackNo
    << (nspc == FrameBuffer::Slot::Delta ? "DELTA" : "KEY")
    << " lambda " << window_.getCurrentWindowSize()
    << " lambda_d " << window_.getDefaultWindowSize()
    << std::endl;
}

//******************************************************************************
void
Pipeliner2::askForRightmostData()
{
    // issue rightmost
    rightmostInterest_ = Interest(keyFramesPrefix_, 1000);
    rightmostInterest_.setMustBeFresh(true);
    rightmostInterest_.setChildSelector(1);
    rightmostInterest_.setMinSuffixComponents(2);
    
    if (exclusionPacket_)
    {
        rightmostInterest_.getExclude().appendAny();
        rightmostInterest_.getExclude().appendComponent(NdnRtcUtils::componentFromInt(exclusionPacket_));
        exclusionPacket_ = 0;
    }
    
    consumer_->getInterestQueue()->enqueueInterest(rightmostInterest_,
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
        frameBuffer_->setState(FrameBuffer::Valid);
        if (useKeyNamespace_)
        { // for video consumer
            keyFrameSeqNo_ = frameNo+1;
            requestNextKey(keyFrameSeqNo_);
            switchToState(StateChasing);
        }
        else
        { // for audio consumer
            deltaFrameSeqNo_ = frameNo+1;
            requestNextDelta(deltaFrameSeqNo_);
            timestamp_ = NdnRtcUtils::millisecondTimestamp();
            switchToState(StateChasing);
            // we don'e need chasing stage if we're not using key frames
            // just ask for subsequent data directly
            askForSubsequentData(data);
        }
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
    
    // normally, we track inter-arrival delay when we receive first segment of
    // a frame. however, Event::FirstSegment can be overriden by Event::Ready
    // if the frame we received consists of just 1 segment. that's why
    // besides FirstSegment events we have to care about Ready events as
    // well - for the frames that made up of 1 segment only
    bool isLegitimateForStabilityTracking =
        event.type_ == FrameBuffer::Event::FirstSegment ||
        (event.type_ == FrameBuffer::Event::Ready &&
         event.slot_->getSegmentsNumber() == 1);
    
    LogTraceC << "Dgen" << STAT_DIV
    << event.slot_->getRecentSegment()->getMetadata().generationDelayMs_ << std::endl;
    
    if (isLegitimateForStabilityTracking)
    {
        // update stability estimator
        stabilityEstimator_.trackInterArrival(frameBuffer_->getCurrentRate());
        (*statStorage_)[Indicator::Darr] = stabilityEstimator_.getLastDelta();
        LogStatC << "Darr"
        << STAT_DIV << (*statStorage_)[Indicator::Darr]
        << STAT_DIV << "Darr mean"
        << STAT_DIV << stabilityEstimator_.getMeanValue()
        << std::endl;

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
        (*statStorage_)[Indicator::RttPrime] = rttChangeEstimator_.getMeanValue();
        LogStatC << "rtt prime"
        << STAT_DIV << (*statStorage_)[Indicator::RttPrime] << std::endl;
    }   
    
    if (event.type_ == FrameBuffer::Event::FirstSegment)
    {
        // update average segments numbers
        updateSegnumEstimation(event.slot_->getNamespace(),
                               event.slot_->getSegmentsNumber(),
                               false);
        updateSegnumEstimation(event.slot_->getNamespace(),
                               event.slot_->getParitySegmentsNumber(),
                               true);
        requestMissing(event.slot_,
                       getInterestLifetime(event.slot_->getPlaybackDeadline(),
                                           event.slot_->getNamespace()),
                       event.slot_->getPlaybackDeadline());
    }
    
    if (event.type_ == FrameBuffer::Event::Ready && !isDeltaFrame)
    {
        // check whether delta frame counter is up-to-date
        // update it if needed
        if (deltaFrameSeqNo_ < event.slot_->getPairedFrameNumber())
        {
            LogWarnC
            << "current delta seq " << deltaFrameSeqNo_
            << " updated to " << event.slot_->getPairedFrameNumber() << std::endl;
            
            deltaFrameSeqNo_ = event.slot_->getPairedFrameNumber();
        }
    }
    
    //**************************************************************************
    int currentMinimalLambda = getCurrentMinimalLambda();
    int currentMaximumLambda = getCurrentMaximumLambda();
    
    if (!window_.isInitialized())
    {
        unsigned int startLambda = (unsigned int)round((double)(currentMaximumLambda + currentMinimalLambda)/2.);
        window_.init(startLambda, frameBuffer_);
        (*statStorage_)[Indicator::DW] = window_.getDefaultWindowSize();
        (*statStorage_)[Indicator::W] = window_.getCurrentWindowSize();
        
        LogTraceC
        << "interest demand was initialized with value " << startLambda
        << " (min " << currentMinimalLambda
        << " max " << currentMaximumLambda
        << "). good luck." << std::endl;
    }
    
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
                    LogDebugC << "increase during chase" << std::endl;
                    
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
                        LogDebugC << "RTT has changed. Waiting"
                        " for RTT stabilization" << std::endl;
                        
                        waitForChange_ = false;
                        waitForStability_ = true;
                        timestamp_ = currentTimestamp;
                    }
                    else if (currentTimestamp-timestamp_ >= timeout)
                    {
                        LogDebugC << "timeout waiting for change" << std::endl;
                        
                        needDecreaseWindow = true;
                        timestamp_ = currentTimestamp;
                    }
                }
                else //if (waitForStability_)
                {
                    LogDebugC << "decrease window" << std::endl;
                    
                    needDecreaseWindow = true;
                    waitForChange_ = true;
                    rttChangeEstimator_.flush();
                    
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
                    
                    LogDebugC << "increase. failed window " << failedWindow_ << std::endl;
                    
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
                bool lambdaTooSmall = (currentMinimalLambda > window_.getDefaultWindowSize());
                bool unstable = !stabilityEstimator_.isStable();
                bool lowBuffer = ((double)frameBuffer_->getPlayableBufferSize() / (double)frameBuffer_->getEstimatedBufferSize() < 0.5);
                // disable draining buffer check
                bool drainingBuffer = false; //(frameBuffer_->getEstimatedBufferSize() < frameBuffer_->getTargetSize());
                
                if (unstable || lowBuffer || drainingBuffer)
                {
                    LogWarnC
                    << "something wrong in fetching mode. unstable: " << unstable
                    << " low buffer: " << lowBuffer
                    << " lambda too small: " << lambdaTooSmall
                    << "(" << currentMinimalLambda  << " vs " << window_.getDefaultWindowSize() << ")"
                    << ". adjusting" << std::endl;
                    
                    switchToState(StateAdjust);
                    needIncreaseWindow = unstable || lowBuffer;
                    waitForStability_ = true;
                    rttChangeEstimator_.flush();
                    
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
        unsigned int currentLambda = window_.getDefaultWindowSize();
        
        if (needIncreaseWindow)
        {
            delta = int(ceil(frac*float(currentLambda)));
            
            if (delta + currentLambda > currentMaximumLambda)
            {
                LogWarnC << "attempt to increase lambda more (by " << delta
                << ") than current maximum lambda allows (" << currentMaximumLambda
                << "). will set lambda to maximum" << std::endl;
                
                delta = currentMaximumLambda - currentLambda;
            }
        }
        else if (needDecreaseWindow)
        {
            delta = -int(ceil(frac*float(currentLambda)));

            if (failedWindow_ >= delta+currentLambda)
            {
                LogDebugC << "trying to decrease lower than fail window "
                << failedWindow_ << std::endl;
                
                switchToState(StateFetching);
                timestamp_ = currentTimestamp;
                delta = 0;
            }
            else
            {
                if (delta+currentLambda < currentMinimalLambda)
                {
                    LogWarnC
                    << "attempt to decrease lambda more (by " << delta
                    << ") than current minimal lambda allows (" << currentMinimalLambda
                    << "). will set lambda to minimal" << std::endl;
                    delta = currentMinimalLambda - currentLambda;
                    
                    if (delta > 0)
                        LogWarnC << "current lambda "
                        << currentLambda
                        << " was smaller than allowed minimal "
                        << currentMinimalLambda
                        << std::endl;
                    
                    switchToState(StateFetching);
                    timestamp_ = currentTimestamp;
                }
            }
        }
        
        LogTraceC << "attempt to change window by " << delta << std::endl;
        
        delta = window_.changeWindow(delta);
        
        (delta ? LogDebugC : LogTraceC)
        << "changed window by " << delta << std::endl;
        
        LogDebugC << "current default window "
        << window_.getDefaultWindowSize() << std::endl;
        
        if (delta)
        {
            (*statStorage_)[Indicator::DW] = window_.getDefaultWindowSize();
            LogStatC << "lambda d"
            << STAT_DIV << (*statStorage_)[Indicator::DW]
            << STAT_DIV << "lambda" << STAT_DIV << (*statStorage_)[Indicator::W]
            << std::endl;
        }
    }
    
    if (isLegitimateForStabilityTracking)
        LogTraceC
        << "\trtt\t" << event.slot_->getRecentSegment()->getRoundTripDelayUsec()/1000.
        << "\trtt stable\t" << rttChangeEstimator_.isStable()
        << "\tlambda\t" << window_.getCurrentWindowSize()
        << "\tlambda_d\t" << window_.getDefaultWindowSize()
        << "\tdarr\t" << stabilityEstimator_.getLastDelta()
        << "\tstable\t" << stabilityEstimator_.isStable()
        << std::endl;
    
    frameBuffer_->synchronizeRelease();
    
    // start playback whenever chasing phase is over and buffer has enough
    // size for playback
    if (state_ > StateChasing &&
        frameBuffer_->getPlayableBufferSize() >= frameBuffer_->getTargetSize())
        if (callback_)
            callback_->onBufferingEnded();
    
    while (window_.canAskForData(deltaFrameSeqNo_))
        requestNextDelta(deltaFrameSeqNo_);
    (*statStorage_)[Indicator::W] = window_.getCurrentWindowSize();
}

unsigned int
Pipeliner2::getCurrentMinimalLambda()
{
    double currentRttEstimation = consumer_->getRttEstimation()->getCurrentEstimation();
    double producerRate = consumer_->getFrameBuffer()->getCurrentRate();
    double packetDelay = (producerRate == 0) ? stabilityEstimator_.getMeanValue() : (1000./producerRate);
    int minimalLambda = (int)round(currentRttEstimation/packetDelay);
    
    if (minimalLambda == 0) minimalLambda = DefaultMinWindow;
    
    return (unsigned int)minimalLambda;
}

unsigned int
Pipeliner2::getCurrentMaximumLambda()
{
    // we just assume that max lambda should be 4 times
    // bigger than current min lambda
    return 4*getCurrentMinimalLambda();
}

void
Pipeliner2::rebuffer()
{
    (*statStorage_)[Indicator::RebufferingsNum]++;
    LogWarnC
    << "rebuffering #" << (*statStorage_)[Indicator::RebufferingsNum]
    << " seed " << seedFrameNo_
    << " key " << keyFrameSeqNo_
    << " delta " << deltaFrameSeqNo_
    << " curent w " << window_.getCurrentWindowSize()
    << " default w " << window_.getDefaultWindowSize()
    << std::endl;
    
    exclusionPacket_ = (useKeyNamespace_)?keyFrameSeqNo_:deltaFrameSeqNo_;
    resetData();
    switchToState(StateWaitInitial);
    askForRightmostData();
}

void
Pipeliner2::updateThreadSyncList(const PrefixMetaInfo& metaInfo, bool isKey)
{
    if (metaInfo.syncList_.size())
    {
        std::map<std::string, int> &mapList = (isKey)?keySyncList_:deltaSyncList_;
        std::map<std::string, int> map;
        
        map.insert(metaInfo.syncList_.begin(), metaInfo.syncList_.end());
        mapList = map;
    }
}

void
Pipeliner2::performThreadTransition()
{
    assert(keySyncList_.find(consumer_->getCurrentThreadName())!= keySyncList_.end());
    assert(deltaSyncList_.find(consumer_->getCurrentThreadName())!= deltaSyncList_.end());
    
    LogTraceC << "thread transition initiated" << std::endl;
    
    std::string
    threadPrefixString = NdnRtcNamespace::getThreadPrefix(consumer_->getPrefix(), consumer_->getCurrentThreadName());
    std::string
    deltaPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString);
    std::string
    keyPrefixString = NdnRtcNamespace::getThreadFramesPrefix(threadPrefixString, useKeyNamespace_);
    
    threadPrefix_ = Name(threadPrefixString.c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString.c_str());
    keyFramesPrefix_ = Name(keyPrefixString.c_str());
    
    deltaSegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgSegNum_);
    keySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgSegNum_);
    deltaParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.deltaAvgParitySegNum_);
    keyParitySegnumEstimatorId_ = NdnRtcUtils::setupMeanEstimator(0, frameSegmentsInfo_.keyAvgParitySegNum_);
    
    int nDeltaPurged = 0, nKeyPurged = 0;
    
    LogTraceC << "purging slots of old thread..." << std::endl;
    
    frameBuffer_->purgeNewSlots(nDeltaPurged, nKeyPurged);
    
    LogTraceC << "purged " << nDeltaPurged << " delta and "
    << nKeyPurged << " key frames" << std::endl;
    
    window_.reset();
    
    keyFrameSeqNo_ = keySyncList_[consumer_->getCurrentThreadName()];
    // it's okay for delta key number to be +1'd
    deltaFrameSeqNo_ = deltaSyncList_[consumer_->getCurrentThreadName()]+1;
    
    requestNextKey(keyFrameSeqNo_);
    requestNextDelta(deltaFrameSeqNo_);
    
    waitForThreadTransition_ = false;
    
    LogTraceC << "thread switched" << std::endl;
}

//******************************************************************************
//******************************************************************************
PipelinerWindow::PipelinerWindow():
isInitialized_(false)
{
}

PipelinerWindow::~PipelinerWindow()
{
}

void
PipelinerWindow::init(unsigned int windowSize, const FrameBuffer* frameBuffer)
{
    isInitialized_ = true;
    frameBuffer_ = frameBuffer;
    framePool_.clear();
    dw_ = windowSize;
    w_ = (int)windowSize;
}

void
PipelinerWindow::reset()
{
    isInitialized_ = false;
}

void
PipelinerWindow::dataArrived(PacketNumber packetNo)
{
    lock_guard<mutex> scopedLock(mutex_);
    
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
    lock_guard<mutex> scopedLock(mutex_);
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

int
PipelinerWindow::changeWindow(int delta)
{
    unsigned int nOutstandingFrames = frameBuffer_->getNewSlotsNum();
    
    if (delta < 0 &&
        abs(delta) >= nOutstandingFrames &&
        nOutstandingFrames > 1)
        delta = -(nOutstandingFrames-1);
    
    if (dw_+delta > 0)
    {
        lock_guard<mutex> scopedLock(mutex_);
        dw_ += delta;
        w_ += delta;
        
        return delta;
    }
    
    return 0;
}
