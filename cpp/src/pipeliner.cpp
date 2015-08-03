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
using namespace ndnrtc::new_api::statistics;
using namespace ndnlog;

const double PipelinerBase::SegmentsAvgNumDelta = 8.;
const double PipelinerBase::SegmentsAvgNumKey = 25.;
const double PipelinerBase::ParitySegmentsAvgNumDelta = 2.;
const double PipelinerBase::ParitySegmentsAvgNumKey = 5.;

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
recoveryCheckpointTimestamp_(0)
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

void
ndnrtc::new_api::PipelinerBase::triggerRebuffering()
{
    rebuffer();
}

void
ndnrtc::new_api::PipelinerBase::threadSwitched()
{
    lock_guard<mutex> scopedLock(streamSwitchMutex_);
    
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
ndnrtc::new_api::PipelinerBase::requestNextKey(PacketNumber& keyFrameNo)
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
ndnrtc::new_api::PipelinerBase::requestNextDelta(PacketNumber& deltaFrameNo)
{
    (*statStorage_)[Indicator::RequestedNum]++;
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
ndnrtc::new_api::PipelinerBase::express(Interest &interest, int64_t priority)
{
    frameBuffer_->interestIssued(interest);
    
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
    streamSwitchMutex_.lock();
    Name packetPrefix(basePrefix);
    streamSwitchMutex_.unlock();
    
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
        }
    }
}

void
ndnrtc::new_api::PipelinerBase::onKeyNeeded(PacketNumber seqNo)
{
    if (keyFrameSeqNo_ <= seqNo)
    {
        keyFrameSeqNo_ = seqNo;
        requestNextKey(keyFrameSeqNo_);
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

#warning check this code
        if (wasTimedOut)
        {
            slot->incremenrRtxNum();
            NdnRtcUtils::frequencyMeterTick(rtxFreqMeterId_);
            (*statStorage_)[Indicator::RtxFrequency] = NdnRtcUtils::currentFrequencyMeterValue(rtxFreqMeterId_);
            (*statStorage_)[Indicator::RtxNum]++;
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
    
    recoveryCheckpointTimestamp_ = 0;
    switchToState(StateInactive);
    keyFrameSeqNo_ = -1;
    deltaFrameSeqNo_ = -1;
    
    frameBuffer_->reset();
}

//******************************************************************************
//******************************************************************************
PipelinerWindow::PipelinerWindow()
{
}

PipelinerWindow::~PipelinerWindow()
{
}

void
PipelinerWindow::init(unsigned int windowSize, const FrameBuffer* frameBuffer)
{
    frameBuffer_ = frameBuffer;
    framePool_.clear();
    dw_ = windowSize;
    w_ = (int)windowSize;
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


//******************************************************************************
//******************************************************************************
Pipeliner2::Pipeliner2(const boost::shared_ptr<Consumer>& consumer,
                       const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
                       const FrameSegmentsInfo& frameSegmentsInfo):
PipelinerBase(consumer, statStorage, frameSegmentsInfo),
stabilityEstimator_(7, 4, 0.15, 0.7),
rttChangeEstimator_(7, 3, 0.12),
dataMeterId_(NdnRtcUtils::setupDataRateMeter(5)),
segmentFreqMeterId_(NdnRtcUtils::setupFrequencyMeter(10)),
exclusionPacket_(0)
{
}

Pipeliner2::~Pipeliner2(){
    
}

int
Pipeliner2::start()
{
    deltaFrameSeqNo_ = -1;
    failedWindow_ = DefaultMinWindow;
    switchToState(StateWaitInitial);
    askForRightmostData();
}

int
Pipeliner2::stop()
{
    switchToState(StateInactive);
    frameBuffer_->release();
    
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
Pipeliner2::onData(const boost::shared_ptr<const Interest>& interest,
                   const boost::shared_ptr<Data>& data)
{
    LogDebugC
    << "data " << data->getName()
    << data->getContent().size() << " bytes" << std::endl;
    
    NdnRtcUtils::dataRateMeterMoreData(dataMeterId_, data->getDefaultWireEncoding().size());
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeterId_);
    recoveryCheckpointTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    
    (*statStorage_)[Indicator::SegmentsReceivedNum]++;
    statStorage_->updateIndicator(Indicator::InRateSegments, NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeterId_));
    (*statStorage_)[Indicator::InBitrateKbps] = NdnRtcUtils::currentDataRateMeterValue(dataMeterId_)*8./1000.;
    
    bool isKeyPrefix = NdnRtcNamespace::isPrefix(data->getName(), keyFramesPrefix_);
    
    switch (state_) {
        case StateWaitInitial:
        {
            // make sure we've got what we expected
            if (rightmostInterest_.getName() == interest->getName())
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
        {
            bool isKeyPrefix = NdnRtcNamespace::isPrefix(interest->getName(), keyFramesPrefix_);

            if (isKeyPrefix)
            {
                PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());

                if (packetNo == keyFrameSeqNo_)
                {
                    LogDebugC << "timeout "
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
        case StateFetching:
        {
            // do something with timeouts
        }
        default:
        {
            // ignore
        }
            break;
    }
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
        window_.init(DefaultWindow, frameBuffer_);
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
        (*statStorage_)[Indicator::RttPrime] = rttChangeEstimator_.getMeanValue();
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
                        
                        rttChangeEstimator_.flush();
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
                else if (waitForStability_)
                {
                    LogDebugC << "decrease window" << std::endl;
                    
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
                bool unstable = !stabilityEstimator_.isStable();
                bool lowBuffer = ((double)frameBuffer_->getPlayableBufferSize() / (double)frameBuffer_->getEstimatedBufferSize() < 0.5);
                // disable draining buffer check
                bool drainingBuffer = false; //(frameBuffer_->getEstimatedBufferSize() < frameBuffer_->getTargetSize());
                
                if (unstable || lowBuffer || drainingBuffer)
                {
                    LogWarnC
                    << "something wrong in fetching mode. unstable: " << unstable
                    << " low buffer: " << lowBuffer
                    << ". adjusting" << std::endl;
                    
                    switchToState(StateAdjust);
                    needIncreaseWindow = unstable || lowBuffer;
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
                LogDebugC << "trying to decrease lower than fail window "
                << failedWindow_ << std::endl;
                
                switchToState(StateFetching);
                timestamp_ = currentTimestamp;
                delta = 0;
            }
        }
        
        LogTraceC << "attempt to change window by " << delta << std::endl;
        
        delta = window_.changeWindow(delta);
        
        (delta ? LogDebugC : LogTraceC)
        << "changed window by " << delta << std::endl;
        
        LogDebugC << "current default window "
        << window_.getDefaultWindowSize() << std::endl;
        
        if (delta)
            (*statStorage_)[Indicator::DW] = window_.getDefaultWindowSize();
    }
    
    if (isLegitimateForStabilityTracking)
        LogTraceC
        << "\trtt\t" << event.slot_->getRecentSegment()->getRoundTripDelayUsec()/1000.
        << "\trtt stable\t" << rttChangeEstimator_.isStable()
        << "\twin\t" << window_.getCurrentWindowSize()
        << "\tdwin\t" << window_.getDefaultWindowSize()
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
