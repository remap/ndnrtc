//
//  consumer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "consumer.h"
#include "frame-buffer.h"
#include "pipeliner.h"
#include "chase-estimation.h"
#include "buffer-estimator.h"
#include "rtt-estimation.h"
#include "playout.h"
#include "ndnrtc-namespace.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;

#define STAT_PRINT(symbol, value) ((symbol) << '\t' << (value) << '\t')

//******************************************************************************
#pragma mark - construction/destruction
Consumer::Consumer(const GeneralParams& generalParams,
                   const GeneralConsumerParams& consumerParams):
generalParams_(generalParams),
consumerParams_(consumerParams),
isConsuming_(false),
rttEstimation_(new RttEstimation()),
chaseEstimation_(new ChaseEstimation()),
bufferEstimator_(new BufferEstimator(rttEstimation_, consumerParams.jitterSizeMs_)),
dataMeterId_(NdnRtcUtils::setupDataRateMeter(5)),
segmentFreqMeterId_(NdnRtcUtils::setupFrequencyMeter(10)),
observerCritSec_(*CriticalSectionWrapper::CreateCriticalSection()),
observer_(NULL)
{
    bufferEstimator_->setRttEstimation(rttEstimation_);
}

Consumer::~Consumer()
{
    if (isConsuming_)
        stop();
    
    observerCritSec_.~CriticalSectionWrapper();
}

//******************************************************************************
#pragma mark - public
int
Consumer::init(const ConsumerSettings& settings)
{
    int res = RESULT_OK;
    
    settings_ = settings;
    streamPrefix_ = *NdnRtcNamespace::getStreamPrefix(settings.userPrefix_, settings_.streamParams_.streamName_);
    interestQueue_.reset(new InterestQueue(settings_.faceProcessor_->getFaceWrapper()));
    
    frameBuffer_.reset(new FrameBuffer(shared_from_this()));
    frameBuffer_->setLogger(logger_);
    frameBuffer_->setDescription(NdnRtcUtils::toString("%s-buffer",
                                                       getDescription().c_str()));
    
    res = frameBuffer_->init();

    if (RESULT_FAIL(res))
        notifyError(-1, "can't initialize frame buffer");
    
#warning error handling!
    chaseEstimation_->setLogger(logger_);
    
#ifdef USE_WINDOW_PIPELINER
    pipeliner_.reset(new Pipeliner2(shared_from_this(),
                                    getCurrentThreadParameters()->getSegmentsInfo()));
#else
    pipeliner_.reset(new Pipeliner(shared_from_this()));
#endif
    
    pipeliner_->initialize();
    pipeliner_->setLogger(logger_);
    pipeliner_->setDescription(NdnRtcUtils::toString("%s-pipeliner",
                                                     getDescription().c_str()));
    pipeliner_->registerCallback(this);
    frameBuffer_->registerCallback(pipeliner_.get());
    
    renderer_->init();
    
    if (observer_)
    {
        CriticalSectionScoped socpedCs(&observerCritSec_);
        observer_->onStatusChanged(ConsumerStatusStopped);
    }
    
    return res;
}

int
Consumer::start()
{
#warning error handling!
    pipeliner_->start();
    
    if (observer_)
    {
        CriticalSectionScoped socpedCs(&observerCritSec_);
        observer_->onStatusChanged(ConsumerStatusNoData);
    }
    
    return RESULT_OK;
}

int
Consumer::stop()
{
#warning error handling!
    pipeliner_->stop();
    playout_->stop();
    renderer_->stopRendering();
    
    if (observer_)
    {
        CriticalSectionScoped socpedCs(&observerCritSec_);
        observer_->onStatusChanged(ConsumerStatusStopped);
    }
    
    return RESULT_OK;
}

void
Consumer::triggerRebuffering()
{
    pipeliner_->triggerRebuffering();
}

void
Consumer::recoveryCheck()
{
#ifdef USE_WINDOW_PIPELINER
    ((Pipeliner2*)pipeliner_.get())->recoveryCheck();
#endif
}

void
Consumer::switchThread(const std::string& threadName)
{
    std::vector<MediaThreadParams*>::iterator it;
    
    for (int i = 0; i < settings_.streamParams_.mediaThreads_.size(); i++)
        if (settings_.streamParams_.mediaThreads_[i]->threadName_ == threadName)
        {
            currentThreadIdx_ = i;
            
            LogInfoC << "thread switched to " << getCurrentThreadName() << std::endl;
            
            NdnRtcUtils::releaseDataRateMeter(dataMeterId_);
            NdnRtcUtils::releaseFrequencyMeter(segmentFreqMeterId_);
            dataMeterId_ = NdnRtcUtils::setupDataRateMeter(5);
            segmentFreqMeterId_ = NdnRtcUtils::setupFrequencyMeter(10);

            pipeliner_->threadSwitched();
            
            if (observer_)
            {
                CriticalSectionScoped scopedCs(&observerCritSec_);
                observer_->onThreadSwitched(threadName);
            }
            
            break;
        }
}

Consumer::State
Consumer::getState() const
{
    switch (pipeliner_->getState()) {
        case Pipeliner::StateBuffering: // fall through
        case Pipeliner::StateChasing:
            return Consumer::StateChasing;
            
        case Pipeliner::StateFetching:
            return Consumer::StateFetching;

        default:
            return Consumer::StateInactive;
    }
}

void
Consumer::getStatistics(ReceiverChannelPerformance& stat) const
{
    memset(&stat, 0, sizeof(stat));
    
    stat.nDataReceived_ = nDataReceived_;
    stat.nTimeouts_ = nTimeouts_;
    
    stat.jitterPlayableMs_ = frameBuffer_->getPlayableBufferSize();
    stat.jitterEstimationMs_ = frameBuffer_->getEstimatedBufferSize();
    stat.jitterTargetMs_ = frameBuffer_->getTargetSize();
    
    stat.segmentsFrequency_ = NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeterId_);
    stat.nBytesPerSec_ = NdnRtcUtils::currentDataRateMeterValue(dataMeterId_);
    stat.actualProducerRate_ = frameBuffer_->getCurrentRate();
    
    interestQueue_->getStatistics(stat);
    
    stat.playoutStat_ = playout_->getStatistics();
    stat.bufferStat_ = frameBuffer_->getStatistics();
    stat.pipelinerStat_ = pipeliner_->getStatistics();
    stat.rttEstimation_ = rttEstimation_->getCurrentEstimation();
    dumpStat(stat);
}

void
Consumer::setLogger(ndnlog::new_api::Logger *logger)
{
    if (frameBuffer_.get())
        frameBuffer_->setLogger(logger);
    
    if (pipeliner_.get())
        pipeliner_->setLogger(logger);
    
    if (playout_.get())
        playout_->setLogger(logger);
    
    if (rateControl_.get())
        rateControl_->setLogger(logger);
    
    if (interestQueue_.get())
        interestQueue_->setLogger(logger);
    
    rttEstimation_->setLogger(logger);
    chaseEstimation_->setLogger(logger);
    bufferEstimator_->setLogger(logger);
    
    ILoggingObject::setLogger(logger);
}

void
Consumer::setDescription(const std::string &desc)
{
    rttEstimation_->setDescription(NdnRtcUtils::toString("%s-rtt-est",
                                                         desc.c_str()));
    chaseEstimation_->setDescription(NdnRtcUtils::toString("%s-chase-est",
                                                           desc.c_str()));
    bufferEstimator_->setDescription(NdnRtcUtils::toString("%s-buffer-est",
                                                           desc.c_str()));
    
    ILoggingObject::setDescription(desc);
}

void
Consumer::onBufferingEnded()
{   
    if (!playout_->isRunning())
    {
        unsigned int targetBufferSize = bufferEstimator_->getTargetSize();
        int adjustment = bufferEstimator_->getTargetSize() - frameBuffer_->getPlayableBufferSize();
        
        if (adjustment < 0)
        {
            LogTraceC
            << "adjusting playback for " << adjustment << std::endl;
        }
        else
        {
            LogWarnC
            << "playback adjustment is positive " << adjustment << std::endl;
            adjustment = 0;
        }
        
        if (getGeneralParameters().useRtx_)
            frameBuffer_->setRetransmissionsEnabled(true);
        
        playout_->start(adjustment);
    }
    
    if (!renderer_->isRendering())
        renderer_->startRendering(settings_.streamParams_.streamName_);
}

void
Consumer::onRebufferingOccurred()
{
    playout_->stop();
    renderer_->stopRendering();
    interestQueue_->reset();
    rttEstimation_->reset();
    
    if (observer_)
    {
        CriticalSectionScoped scopedCs(&observerCritSec_);
        observer_->onRebufferingOccurred();
    }
}

void
Consumer::onStateChanged(const int &oldState, const int &newState)
{
    if (observer_)
    {
        CriticalSectionScoped scopedCs(&observerCritSec_);
        ConsumerStatus status;
        
        switch (newState) {
            case Pipeliner::StateChasing:
                status = ConsumerStatusChasing;
                break;
                
            case Pipeliner::StateBuffering:
                status = ConsumerStatusBuffering;
                break;
                
            case Pipeliner::StateFetching:
                status = ConsumerStatusFetching;
                break;
                
            case Pipeliner::StateInactive:
            default:
                status = ConsumerStatusStopped;
                break;
        }
        
        observer_->onStatusChanged(status);
    }
}

void
Consumer::onInitialDataArrived()
{
    if (observer_)
    {
        CriticalSectionScoped scopedCs(&observerCritSec_);
        observer_->onStatusChanged(ConsumerStatusChasing);
    }
}

void
Consumer::dumpStat(ReceiverChannelPerformance stat) const
{
    LogStatC << STAT_DIV
    << SYMBOL_SEG_RATE << STAT_DIV << stat.segmentsFrequency_ << STAT_DIV
    << SYMBOL_INTEREST_RATE << STAT_DIV << stat.interestFrequency_ << STAT_DIV
    << SYMBOL_PRODUCER_RATE << STAT_DIV << stat.actualProducerRate_ << STAT_DIV
    << SYMBOL_JITTER_TARGET << STAT_DIV << stat.jitterTargetMs_ << STAT_DIV
    << SYMBOL_JITTER_ESTIMATE << STAT_DIV << stat.jitterEstimationMs_ << STAT_DIV
    << SYMBOL_JITTER_PLAYABLE << STAT_DIV << stat.jitterPlayableMs_ << STAT_DIV
    << SYMBOL_INRATE << STAT_DIV << stat.nBytesPerSec_*8/1000 << STAT_DIV
    << SYMBOL_NREBUFFER << STAT_DIV << stat.pipelinerStat_.nRebuffer_ << STAT_DIV
    // playout stat
    << SYMBOL_NPLAYED << STAT_DIV << stat.playoutStat_.nPlayed_ << STAT_DIV
    << SYMBOL_NPLAYEDKEY << STAT_DIV << stat.playoutStat_.nPlayedKey_ << STAT_DIV
    << SYMBOL_NSKIPPEDNOKEY << STAT_DIV << stat.playoutStat_.nSkippedNoKey_ << STAT_DIV
    << SYMBOL_NSKIPPEDINC << STAT_DIV << stat.playoutStat_.nSkippedIncomplete_ << STAT_DIV
    << SYMBOL_NSKIPPEDINCKEY << STAT_DIV << stat.playoutStat_.nSkippedIncompleteKey_ << STAT_DIV
    << SYMBOL_NSKIPPEDGOP << STAT_DIV << stat.playoutStat_.nSkippedInvalidGop_ << STAT_DIV
    // buffer stat
    << SYMBOL_NACQUIRED << STAT_DIV << stat.bufferStat_.nAcquired_ << STAT_DIV
    << SYMBOL_NACQUIREDKEY << STAT_DIV << stat.bufferStat_.nAcquiredKey_ << STAT_DIV
    << SYMBOL_NDROPPED << STAT_DIV << stat.bufferStat_.nDropped_ << STAT_DIV
    << SYMBOL_NDROPPEDKEY << STAT_DIV << stat.bufferStat_.nDroppedKey_ << STAT_DIV
    << SYMBOL_NASSEMBLED << STAT_DIV << stat.bufferStat_.nAssembled_ << STAT_DIV
    << SYMBOL_NASSEMBLEDKEY << STAT_DIV << stat.bufferStat_.nAssembledKey_ << STAT_DIV
    << SYMBOL_NRESCUED << STAT_DIV << stat.bufferStat_.nRescued_ << STAT_DIV
    << SYMBOL_NRESCUEDKEY << STAT_DIV << stat.bufferStat_.nRescuedKey_ << STAT_DIV
    << SYMBOL_NRECOVERED << STAT_DIV << stat.bufferStat_.nRecovered_ << STAT_DIV
    << SYMBOL_NRECOVEREDKEY << STAT_DIV << stat.bufferStat_.nRecoveredKey_ << STAT_DIV
    << SYMBOL_NINCOMPLETE << STAT_DIV << stat.bufferStat_.nIncomplete_ << STAT_DIV
    << SYMBOL_NINCOMPLETEKEY << STAT_DIV << stat.bufferStat_.nIncompleteKey_ << STAT_DIV

    << SYMBOL_NRTX << STAT_DIV << stat.pipelinerStat_.nRtx_ << STAT_DIV
    << SYMBOL_AVG_DELTA << STAT_DIV << stat.pipelinerStat_.avgSegNum_ << STAT_DIV
    << SYMBOL_AVG_KEY << STAT_DIV << stat.pipelinerStat_.avgSegNumKey_ << STAT_DIV
    << SYMBOL_RTT_EST << STAT_DIV << stat.rttEstimation_ << STAT_DIV
    << SYMBOL_NINTRST << STAT_DIV << stat.pipelinerStat_.nInterestSent_ << STAT_DIV
    << SYMBOL_NREQUESTED << STAT_DIV << stat.pipelinerStat_.nRequested_ << STAT_DIV
    << SYMBOL_NREQUESTEDKEY << STAT_DIV << stat.pipelinerStat_.nRequestedKey_ << STAT_DIV
    << SYMBOL_NDATA << STAT_DIV << stat.nDataReceived_ << STAT_DIV
    << SYMBOL_NTIMEOUT << STAT_DIV << stat.nTimeouts_ << STAT_DIV
    << SYMBOL_LATENCY << STAT_DIV << stat.playoutStat_.latency_
    << std::endl;
}

IPacketAssembler*
Consumer::getPacketAssembler()
{
#ifdef USE_WINDOW_PIPELINER
    return (Pipeliner2*)pipeliner_.get();
#else
    return this;
#endif
}
//******************************************************************************
#pragma mark - private
void Consumer::onData(const shared_ptr<const Interest>& interest,
            const shared_ptr<Data>& data)
{
    LogTraceC << "data " << data->getName() << std::endl;
    nDataReceived_++;
    
    NdnRtcUtils::dataRateMeterMoreData(dataMeterId_, data->getDefaultWireEncoding().size());
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeterId_);
    
    frameBuffer_->newData(*data);
}
void Consumer::onTimeout(const shared_ptr<const Interest>& interest)
{
    nTimeouts_++;
    frameBuffer_->interestTimeout(*interest);
}
