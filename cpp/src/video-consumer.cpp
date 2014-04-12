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
                             const shared_ptr<InterestQueue>& interestQueue):
Consumer(params, interestQueue),
renderer_(new NdnRenderer(1, params)),
decoder_(new NdnVideoDecoder(params))
{
    setDescription("vconsumer");
    decoder_->setFrameConsumer(renderer_.get());
}

VideoConsumer::~VideoConsumer()
{
}

//******************************************************************************
#pragma mark - public
int
VideoConsumer::init()
{
#warning error handling!
    Consumer::init();

    pipeliner_->setDescription(NdnRtcUtils::toString("%s-pipeliner",
                                                     getDescription().c_str()));
    
    renderer_->init();
    decoder_->init();
    
    playout_.reset(new VideoPlayout(shared_from_this()));
    playout_->setLogger(logger_);
    playout_->init(decoder_.get());
    
    LogInfoC << "initialized" << endl;
    return RESULT_OK;
}

int
VideoConsumer::start()
{
#warning error handling!
    renderer_->startRendering(string(params_.producerId));
    
    Consumer::start();
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
VideoConsumer::stop()
{
#warning error handling!
    Consumer::stop();
    
    renderer_->stopRendering();
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

void
VideoConsumer::reset()
{
    Consumer::reset();
    decoder_->init();
    
    playout_->stop();
    playout_->init(decoder_.get());
    playout_->start();
}

void
VideoConsumer::setLogger(ndnlog::new_api::Logger *logger)
{
    renderer_->setLogger(logger);
    decoder_->setLogger(logger);
    
    Consumer::setLogger(logger);
}

//******************************************************************************
#pragma mark - construction/destruction