//
//  video-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "video-playout.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
VideoPlayout::VideoPlayout(const shared_ptr<const Consumer>& consumer):
Playout(consumer)
{
    
}

VideoPlayout::~VideoPlayout()
{
    
}

//******************************************************************************
#pragma mark - public

//******************************************************************************
#pragma mark - private
bool
VideoPlayout::playbackPacket(int64_t packetTsLocal)
{
    bool res = false;
    jitterTiming_.startFramePlayout();
    webrtc::EncodedImage frame;
    
    if (data_)
    {
        delete data_;
        data_ = nullptr;
    }
    
    frameBuffer_->acquireSlot(&data_);
    
    // render frame if we have one
    if (data_ && frameConsumer_)
    { 
        ((NdnFrameData*)data_)->getFrame(frame);
        frameConsumer_->onEncodedFrameDelivered(frame);
        res = true;
    }
    
    // get playout time (delay) for the rendered frame
    int framePlayoutTime = frameBuffer_->releaseAcquiredSlot();
    
    assert(framePlayoutTime >= 0);
    
    LogTraceC
    << "playout time " << framePlayoutTime
    << " data: " << (data_?"YES":"NO") << endl;
    
    if (framePlayoutTime >= 0)
    {
        jitterTiming_.updatePlayoutTime(framePlayoutTime);
        
        // setup and run playout timer for calculated playout interval
        jitterTiming_.runPlayoutTimer();
    }
    
    return res;
}