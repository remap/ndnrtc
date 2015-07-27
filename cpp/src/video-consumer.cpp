//
//  video-consumer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "video-consumer.h"
#include "frame-buffer.h"
#include "pipeliner.h"
#include "chase-estimation.h"
#include "buffer-estimator.h"
#include "rtt-estimation.h"
#include "video-playout.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
VideoConsumer::VideoConsumer(const GeneralParams& generalParams,
                             const GeneralConsumerParams& consumerParams,
                             IExternalRenderer* const externalRenderer):
Consumer(generalParams, consumerParams),
decoder_(new NdnVideoDecoder())
{
    setDescription("vconsumer");
    renderer_.reset(new ExternalVideoRendererAdaptor(externalRenderer));
    decoder_->setFrameConsumer(getRenderer().get());
}

VideoConsumer::~VideoConsumer()
{
    
}

//******************************************************************************
#pragma mark - public
int
VideoConsumer::init(const ConsumerSettings& settings,
                    const std::string& threadName)
{
    int res = RESULT_OK;
    
    LogInfoC << "unix timestamp: " << std::fixed << std::setprecision(6)
    << NdnRtcUtils::unixTimestamp() << std::endl;

    if (RESULT_GOOD(Consumer::init(settings, threadName)))
    {
        pipeliner_->setUseKeyNamespace(true);
        pipeliner_->initialize();
        
        interestQueue_->registerCallback(this);
        decoder_->init(((VideoThreadParams*)getCurrentThreadParameters())->coderParams_);
        
        playout_.reset(new VideoPlayout(this, statStorage_));
        playout_->setLogger(logger_);
        playout_->init(decoder_.get());
        ((VideoPlayout*)playout_.get())->onFrameSkipped_ = boost::bind(&VideoConsumer::onFrameSkipped, this, _1, _2, _3, _4, _5);
        
#if 0
        rateControl_.reset(new RateControl(shared_from_this()));
        
        if (RESULT_FAIL(rateControl_->initialize(params_)))
        {
            res = RESULT_ERR;
            LogErrorC << "failed to initialize rate control" << std::endl;
        }
        else
        {
            getFrameBuffer()->setRateControl(rateControl_);
#endif
            LogInfoC << "initialized" << std::endl;
#if 0
        }
#endif
        
        return res;
    }
    
    return RESULT_ERR;
}

int
VideoConsumer::start()
{
#warning error handling!
    Consumer::start();
    
    LogInfoC << "started" << std::endl;
    return RESULT_OK;
}

int
VideoConsumer::stop()
{
#warning error handling!
    Consumer::stop();
    
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
VideoConsumer::setLogger(ndnlog::new_api::Logger *logger)
{
    getRenderer()->setLogger(logger);
    decoder_->setLogger(logger);
    
    Consumer::setLogger(logger);
}

void
VideoConsumer::onInterestIssued(const shared_ptr<const ndn::Interest>& interest)
{
#if 0
    if (rateControl_.get())
        rateControl_->interestExpressed(interest);
#endif
}

void
VideoConsumer::onStateChanged(const int& oldState, const int& newState)
{
    Consumer::onStateChanged(oldState, newState);
#if 0
    if (rateControl_.get())
    {
        if (newState == Pipeliner::StateFetching)
            rateControl_->start();
        
        if (oldState == Pipeliner::StateFetching)
            rateControl_->stop();
    }
#endif
}

void
VideoConsumer::playbackEventOccurred(PlaybackEvent event,
                                     unsigned int frameSeqNo)
{
    if (observer_)
    {
        lock_guard<mutex> scopedLock(observerMutex_);
        observer_->onPlaybackEventOccurred(event, frameSeqNo);
    }
}

void
VideoConsumer::triggerRebuffering()
{
    Consumer::triggerRebuffering();
    decoder_->reset();
}

//******************************************************************************
void
VideoConsumer::onFrameSkipped(PacketNumber playbackNo, PacketNumber sequenceNo,
                              PacketNumber pairedNo, bool isKey,
                              double assembledLevel)
{
    // empty
}

void
VideoConsumer::onTimeout(const shared_ptr<const Interest>& interest)
{
#if 0
    if (rateControl_.get())
        rateControl_->interestTimeout(interest);
#endif
}


