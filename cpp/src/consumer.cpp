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

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

#define STAT_PRINT(symbol, value) ((symbol) << '\t' << (value) << '\t')

//******************************************************************************
#pragma mark - construction/destruction
Consumer::Consumer(const ParamsStruct& params,
                   const shared_ptr<InterestQueue>& interestQueue,
                   const shared_ptr<RttEstimation>& rttEstimation):
ndnrtc::NdnRtcObject(params),
isConsuming_(false),
interestQueue_(interestQueue),
rttEstimation_(rttEstimation),
chaseEstimation_(new ChaseEstimation()),
bufferEstimator_(new BufferEstimator()),
dataMeterId_(NdnRtcUtils::setupDataRateMeter(5)),
segmentFreqMeterId_(NdnRtcUtils::setupFrequencyMeter(10))
{
    if (!rttEstimation.get())
    {
        rttEstimation_.reset(new RttEstimation());
    }
    
    bufferEstimator_->setRttEstimation(rttEstimation_);
    bufferEstimator_->setMinimalBufferSize(params.jitterSize);
}

Consumer::~Consumer()
{
    if (isConsuming_)
        stop();
}

//******************************************************************************
#pragma mark - public
int
Consumer::init()
{
    int res = RESULT_OK;
    
    if (!interestQueue_.get() ||
        !rttEstimation_.get())
        return notifyError(-1, "");
    
    frameBuffer_.reset(new ndnrtc::new_api::FrameBuffer(shared_from_this()));
    frameBuffer_->setLogger(logger_);
    frameBuffer_->setDescription(NdnRtcUtils::toString("%s-buffer",
                                                       getDescription().c_str()));
    
    res = frameBuffer_->init();

    if (RESULT_FAIL(res))
        notifyError(-1, "can't initialize frame buffer");
    
#warning error handling!
    chaseEstimation_->setLogger(logger_);
    
    pipeliner_.reset(new Pipeliner(shared_from_this()));
    pipeliner_->setLogger(logger_);
    pipeliner_->setDescription(NdnRtcUtils::toString("%s-pipeliner",
                                                     getDescription().c_str()));
    pipeliner_->registerCallback(this);
    
    renderer_->init();
    
    return res;
}

int
Consumer::start()
{
#warning error handling!
    pipeliner_->start();
    
    return RESULT_OK;
}

int
Consumer::stop()
{
#warning error handling!
    pipeliner_->stop();
    playout_->stop();
    renderer_->stopRendering();
    
    return RESULT_OK;
}

void
Consumer::reset()
{
    interestQueue_->reset();
}

void
Consumer::triggerRebuffering()
{
    pipeliner_->triggerRebuffering();
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
    
    stat.nInterestSent_ = pipeliner_->getInterestNum();
    stat.nDataReceived_ = nDataReceived_;
    stat.nTimeouts_ = nTimeouts_;
    
    stat.segNumDelta_ = pipeliner_->getAvgSegNum(false);
    stat.segNumKey_ = pipeliner_->getAvgSegNum(true);
    stat.rtxNum_ = pipeliner_->getRtxNum();
    stat.rtxFreq_ = pipeliner_->getRtxFreq();
    stat.rebufferingEvents_ = pipeliner_->getRebufferingNum();
    stat.rttEstimation_ = rttEstimation_->getCurrentEstimation();
    
    stat.jitterPlayableMs_ = frameBuffer_->getPlayableBufferSize();
    stat.jitterEstimationMs_ = frameBuffer_->getEstimatedBufferSize();
    stat.jitterTargetMs_ = frameBuffer_->getTargetSize();
    
    stat.segmentsFrequency_ = NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeterId_);
    stat.nBytesPerSec_ = NdnRtcUtils::currentDataRateMeterValue(dataMeterId_);
    stat.actualProducerRate_ = frameBuffer_->getCurrentRate();
    
    interestQueue_->getStatistics(stat);
    stat.playoutStat_ = playout_->getStatistics();
    stat.bufferStat_ = frameBuffer_->getStatistics();
    
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
        playout_->start();
    
    if (!renderer_->isRendering())
        renderer_->startRendering(std::string(params_.producerId));
}

void
Consumer::onRebufferingOccurred()
{
    reset();

    renderer_->stopRendering();
    rttEstimation_->reset();
}

void
Consumer::onStateChanged(const int &oldState, const int &newState)
{
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
    << SYMBOL_NREBUFFER << STAT_DIV << stat.rebufferingEvents_ << STAT_DIV
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

    << SYMBOL_NRTX << STAT_DIV << stat.rtxNum_ << STAT_DIV
    << SYMBOL_AVG_DELTA << STAT_DIV << stat.segNumDelta_ << STAT_DIV
    << SYMBOL_AVG_KEY << STAT_DIV << stat.segNumKey_ << STAT_DIV
    << SYMBOL_RTT_EST << STAT_DIV << stat.rttEstimation_ << STAT_DIV
    << SYMBOL_NINTRST << STAT_DIV << stat.nInterestSent_ << STAT_DIV
    << SYMBOL_NDATA << STAT_DIV << stat.nDataReceived_ << STAT_DIV
    << SYMBOL_NTIMEOUT << STAT_DIV << stat.nTimeouts_
    << std::endl;
}

//******************************************************************************
#pragma mark - private
void Consumer::onData(const shared_ptr<const Interest>& interest,
            const shared_ptr<Data>& data)
{
    LogTraceC << "data " << data->getName() << std::endl;
    nDataReceived_++;
    
    NdnRtcUtils::dataRateMeterMoreData(dataMeterId_, data->getContent().size());
    NdnRtcUtils::frequencyMeterTick(segmentFreqMeterId_);
    
    frameBuffer_->newData(*data);
}
void Consumer::onTimeout(const shared_ptr<const Interest>& interest)
{
    nTimeouts_++;
    frameBuffer_->interestTimeout(*interest);
}
