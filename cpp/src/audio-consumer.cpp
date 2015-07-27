//
//  audio-consumer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "audio-consumer.h"
#include "audio-playout.h"
#include "pipeliner.h"

using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost;

//******************************************************************************
#pragma mark - construction/destruction
AudioConsumer::AudioConsumer(const GeneralParams& generalParams,
                             const GeneralConsumerParams& consumerParams):
Consumer(generalParams, consumerParams)
{
    renderer_.reset(new AudioRenderer());
    setDescription("aconsumer");
}

AudioConsumer::~AudioConsumer()
{
}

//******************************************************************************
#pragma mark - public
int
AudioConsumer::init(const ConsumerSettings& settings,
                    const std::string& threadName)
{
    if (RESULT_GOOD(Consumer::init(settings, threadName)))
    {
        pipeliner_->setUseKeyNamespace(false);
        pipeliner_->initialize();
        
        playout_.reset(new AudioPlayout(this, statStorage_));
        playout_->setLogger(logger_);
        playout_->init(renderer_.get());
        
        LogInfoC << "initialized" << std::endl;
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

int
AudioConsumer::start()
{
    if (RESULT_GOOD(Consumer::start()))
    {
        LogInfoC << "started" << std::endl;
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

int
AudioConsumer::stop()
{
    Consumer::stop();
    LogInfoC << "stopped" << std::endl;
    return RESULT_OK;
}

void
AudioConsumer::setLogger(ndnlog::new_api::Logger *logger)
{
    getRenderer()->setLogger(logger);
    Consumer::setLogger(logger);
}
