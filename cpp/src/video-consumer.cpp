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

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
VideoConsumer::VideoConsumer(const ParamsStruct& params,
                             const shared_ptr<InterestQueue>& interestQueue,
                             const shared_ptr<RttEstimation>& rttEstimation):
Consumer(params, interestQueue, rttEstimation),
decoder_(new NdnVideoDecoder(params))
{
    setDescription("vconsumer");
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
    LogInfoC << "unix timestamp: " << fixed << setprecision(6) << NdnRtcUtils::unixTimestamp() << endl;    
#warning error handling!
    Consumer::init();

    decoder_->init();
    
    playout_.reset(new VideoPlayout(this));
    playout_->setLogger(logger_);
    playout_->init(decoder_.get());
    
    LogInfoC << "initialized" << endl;
    
    return RESULT_OK;
}

int
VideoConsumer::start()
{
#warning error handling!
    Consumer::start();
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
VideoConsumer::stop()
{
#warning error handling!
    Consumer::stop();
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

void
VideoConsumer::reset()
{
    Consumer::reset();
    
    playout_->stop();
    playout_->init(decoder_.get());
    playout_->start();
}

void
VideoConsumer::setLogger(ndnlog::new_api::Logger *logger)
{
    getRenderer()->setLogger(logger);
    decoder_->setLogger(logger);
    
    Consumer::setLogger(logger);
}

