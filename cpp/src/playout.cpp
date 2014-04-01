//
//  playout.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#include "playout.h"
#include "ndnrtc-utils.h"

using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
Playout::Playout(const shared_ptr<const Consumer> &consumer):
isRunning_(false),
consumer_(consumer),
playoutThread_(*webrtc::ThreadWrapper::CreateThread(Playout::playoutThreadRoutine, this)),
frameConsumer_(nullptr),
data_(nullptr)
{
    setDescription("playout");
    jitterTiming_.flush();
    
    if (consumer_.get())
    {
        frameBuffer_ = consumer_->getFrameBuffer();
    }
}

Playout::~Playout()
{
    if (isRunning_)
        stop();
}

//******************************************************************************
#pragma mark - public
int
Playout::init(IEncodedFrameConsumer* consumer)
{
    jitterTiming_.flush();
    frameConsumer_ = consumer;
}

int
Playout::start()
{
    nPlayed_ = 0;
    nMissed_ = 0;
    nIncomplete_ = 0;
    isRunning_ = true;
    
    unsigned int tid;
    playoutThread_.Start(tid);
}

int
Playout::stop()
{
    playoutThread_.SetNotAlive();
    isRunning_ = false;
    playoutThread_.Stop();
}

void
Playout::setLogger(ndnlog::new_api::Logger *logger)
{
    jitterTiming_.setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
Playout::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_.setDescription(NdnRtcUtils::toString("%s-timing",
                                                       getDescription().c_str()));
}

void
Playout::getStatistics(ReceiverChannelPerformance& stat)
{
    stat.nPlayed_ = nPlayed_;
    stat.nMissed_ = nMissed_;
    stat.nIncomplete_ = nIncomplete_;
}

//******************************************************************************
#pragma mark - private
bool
Playout::processPlayout()
{
    if (isRunning_)
    {
        int64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (frameBuffer_->getState() == FrameBuffer::Valid)
        {
            if (playbackPacket(now))
            {
                nPlayed_++;
                
                LogStatC << "\tplay\t" << nPlayed_ << endl;
            }
            else
            {
                nMissed_++;
                
                LogStatC << "\tmissed\t" << nMissed_ << endl;
            }
        }
    }
    
    return isRunning_;
}