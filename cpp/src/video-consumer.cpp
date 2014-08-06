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
VideoConsumer::VideoConsumer(const ParamsStruct& params,
                             const shared_ptr<InterestQueue>& interestQueue,
                             const shared_ptr<RttEstimation>& rttEstimation,
                             IExternalRenderer* const externalRenderer):
Consumer(params, interestQueue, rttEstimation),
decoder_(new NdnVideoDecoder(params.streamsParams[0]))
{
    setDescription("vconsumer");
    interestQueue_->registerCallback(this);
    
    if (externalRenderer)
        renderer_.reset(new ExternalVideoRendererAdaptor(externalRenderer));
    else
        renderer_.reset(new VideoRenderer(1, params));
    
    decoder_->setFrameConsumer(getRenderer().get());
}

VideoConsumer::~VideoConsumer()
{
    
}

//******************************************************************************
#pragma mark - public
int
VideoConsumer::init()
{
    LogInfoC << "unix timestamp: " << std::fixed << std::setprecision(6)
    << NdnRtcUtils::unixTimestamp() << std::endl;

    if (RESULT_GOOD(Consumer::init()))
    {
        int res = RESULT_OK;
        
        decoder_->init();
        
        playout_.reset(new VideoPlayout(this));
        playout_->setLogger(logger_);
        playout_->init(decoder_.get());
        
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
    if (rateControl_.get())
        rateControl_->interestExpressed(interest);
}

void
VideoConsumer::onStateChanged(const int& oldState, const int& newState)
{
    if (rateControl_.get())
    {
        if (newState == Pipeliner::StateFetching)
            rateControl_->start();
        
        if (oldState == Pipeliner::StateFetching)
            rateControl_->stop();
    }
}

void
VideoConsumer::onTimeout(const shared_ptr<const Interest>& interest)
{
    if (rateControl_.get())
        rateControl_->interestTimeout(interest);
}
