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

using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
Playout::Playout(const shared_ptr<const FetchChannel> &fetchChannel):
isRunning_(false),
fetchChannel_(fetchChannel),
playoutThread_(*webrtc::ThreadWrapper::CreateThread(Playout::playoutThreadRoutine, this)),
frameConsumer_(nullptr),
data_(nullptr)
{
//    jitterTiming_.setLogger(new NdnLogger("jitter-timing.log"));
    jitterTiming_.flush();
    
    if (fetchChannel_.get())
    {
        frameBuffer_ = fetchChannel_->getFrameBuffer();
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
            playbackPacket(now);
        }
    }
    
    return isRunning_;
}