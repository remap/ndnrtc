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
#include "renderer.h"
#include "interest-queue.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;

#define STAT_PRINT(symbol, value) ((symbol) << '\t' << (value) << '\t')

const int Consumer::MaxIdleTimeMs = 500;
const int Consumer::MaxChasingTimeMs = 6*Consumer::MaxIdleTimeMs;

//******************************************************************************
#pragma mark - construction/destruction
Consumer::Consumer(const GeneralParams& generalParams,
                   const GeneralConsumerParams& consumerParams):
statStorage_(statistics::StatisticsStorage::createConsumerStatistics()),
generalParams_(generalParams),
consumerParams_(consumerParams),
isConsuming_(false),
rttEstimation_(new RttEstimation(statStorage_)),
chaseEstimation_(new ChaseEstimation()),
bufferEstimator_(new BufferEstimator(rttEstimation_, consumerParams.jitterSizeMs_)),
observer_(NULL)
{
    bufferEstimator_->setRttEstimation(rttEstimation_);
}

Consumer::~Consumer()
{
    if (isConsuming_)
        stop();
}

//******************************************************************************
#pragma mark - public
int
Consumer::init(const ConsumerSettings& settings,
               const std::string& threadName)
{
    int res = RESULT_OK;
    
    settings_ = settings;
    streamPrefix_ = *NdnRtcNamespace::getStreamPrefix(settings.userPrefix_, settings_.streamParams_.streamName_);
    interestQueue_.reset(new InterestQueue(settings_.faceProcessor_->getFaceWrapper(),
                                           statStorage_));
    
    frameBuffer_.reset(new FrameBuffer(dynamic_pointer_cast<Consumer>(shared_from_this()), statStorage_));
    frameBuffer_->setLogger(logger_);
    frameBuffer_->setDescription(NdnRtcUtils::toString("%s-buffer",
                                                       getDescription().c_str()));
    
    res = frameBuffer_->init();

    if (RESULT_FAIL(res))
        notifyError(-1, "can't initialize frame buffer");
    
    currentThreadIdx_ = getThreadIdx(threadName);
    
    if (currentThreadIdx_ < 0)
    {
        notifyError(-1, "couldn't find thread with name %s", threadName.c_str());
        currentThreadIdx_ = 0;
    }
    
    chaseEstimation_->setLogger(logger_);
    pipeliner_.reset(new Pipeliner2(dynamic_pointer_cast<Consumer>(shared_from_this()),
                                    statStorage_,
                                    getCurrentThreadParameters()->getSegmentsInfo()));
    pipeliner_->setLogger(logger_);
    pipeliner_->setDescription(NdnRtcUtils::toString("%s-pipeliner",
                                                     getDescription().c_str()));
    pipeliner_->registerCallback(this);
    frameBuffer_->registerCallback(pipeliner_.get());
    
    renderer_->init();
    
    if (observer_)
    {
        lock_guard<mutex> scopedLock(observerMutex_);
        observer_->onStatusChanged(ConsumerStatusStopped);
    }
    
    return res;
}

int
Consumer::start()
{
    isConsuming_ = true;

    if (RESULT_FAIL(pipeliner_->start()))
    {
        isConsuming_ = false;
        return RESULT_ERR;
    }
    
    if (observer_)
    {
        lock_guard<mutex> scopedLock(observerMutex_);
        observer_->onStatusChanged(ConsumerStatusNoData);
    }
    
    return RESULT_OK;
}

int
Consumer::stop()
{
    NdnRtcUtils::performOnBackgroundThread([this]()->void{
        isConsuming_ = false;
        pipeliner_->stop();
        playout_->stop();
        renderer_->stopRendering();
    });
    
    LogStatC << "final statistics:\n" << getStatistics() << std::endl;

    return RESULT_OK;
}

int
Consumer::getIdleTime()
{
    return pipeliner_->getIdleTime();
}

void
Consumer::triggerRebuffering()
{
    LogWarnC
    << "rebuffering triggered. idle time "
    << getIdleTime() << std::endl;
    
    interestQueue_->reset();
    playout_->stop();
    renderer_->stopRendering();
    pipeliner_->triggerRebuffering();
    rttEstimation_->reset();
    
    if (observer_)
    {
        lock_guard<mutex> scopedLock(observerMutex_);
        observer_->onRebufferingOccurred();
    }
    
}

void
Consumer::switchThread(const std::string& threadName)
{
    std::vector<MediaThreadParams*>::iterator it;
    int idx = getThreadIdx(threadName);
    
    if (idx >= 0)
    {
        currentThreadIdx_ = idx;
        
        LogInfoC << "thread switching to " << getCurrentThreadName()
        << " initiated" << std::endl;
        
        if (!pipeliner_->threadSwitched())
            playout_->stop();
        
        if (observer_)
        {
            lock_guard<mutex> scopedLock(observerMutex_);
            observer_->onThreadSwitched(threadName);
        }
    }
}

Consumer::State
Consumer::getState() const
{
    switch (pipeliner_->getState()) {
        case Pipeliner2::StateBuffering: // fall through
        case Pipeliner2::StateChasing:
            return Consumer::StateChasing;
            
        case Pipeliner2::StateAdjust:
        case Pipeliner2::StateFetching:
            return Consumer::StateFetching;

        default:
            return Consumer::StateInactive;
    }
}

statistics::StatisticsStorage
Consumer::getStatistics() const
{
    (*statStorage_)[statistics::Indicator::Timestamp] = NdnRtcUtils::millisecondTimestamp();
    return *statStorage_;
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
    // start rendering first, as playout may supply frames
    // right after "start playout" has been called
    if (!renderer_->isRendering())
        renderer_->startRendering(settings_.streamParams_.streamName_);
    
    if (!playout_->isRunning())
    {
        unsigned int targetBufferSize = bufferEstimator_->getTargetSize();
        int adjustment = targetBufferSize - frameBuffer_->getPlayableBufferSize();
        
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
}

void
Consumer::onStateChanged(const int &oldState, const int &newState)
{
    if (observer_)
    {
        lock_guard<mutex> scopedLock(observerMutex_);
        ConsumerStatus status;
        
        switch (newState) {
            case Pipeliner2::StateWaitInitial:
            case Pipeliner2::StateChasing:
                status = ConsumerStatusNoData;
                break;
            case Pipeliner2::StateAdjust:
                status = ConsumerStatusAdjusting;
                break;
            
            case Pipeliner2::StateBuffering:
                status = ConsumerStatusBuffering;
                break;
                
            case Pipeliner2::StateFetching:
                status = ConsumerStatusFetching;
                break;
                
            case Pipeliner2::StateInactive:
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
        lock_guard<mutex> scopedLock(observerMutex_);
        observer_->onStatusChanged(ConsumerStatusAdjusting);
    }
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
int
Consumer::getThreadIdx(const std::string& threadName)
{
    int idx = -1;
    
    for (int i = 0; i < settings_.streamParams_.mediaThreads_.size(); i++)
        if (settings_.streamParams_.mediaThreads_[i]->threadName_ == threadName)
        {
            idx = i;
        }
    
    return idx;
}
