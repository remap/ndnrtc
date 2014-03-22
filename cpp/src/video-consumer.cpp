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
    
    renderer_->init();
    decoder_->init();
    
    playout_.reset(new VideoPlayout(shared_from_this()));
    playout_->init(decoder_.get());
    
    return RESULT_OK;
}

int
VideoConsumer::start()
{
#warning error handling!
    renderer_->startRendering(string(params_.producerId));
    
    Consumer::start();
    
    return RESULT_OK;
}

int
VideoConsumer::stop()
{
#warning error handling!
    Consumer::stop();
    
    renderer_->stopRendering();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - construction/destruction