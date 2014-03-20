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
VideoPlayout::VideoPlayout(const shared_ptr<const FetchChannel>& fetchChannel):
Playout(fetchChannel)
{
    
}

VideoPlayout::~VideoPlayout()
{
    
}

//******************************************************************************
#pragma mark - public

//******************************************************************************
#pragma mark - private
void
VideoPlayout::playbackPacket(int64_t packetTsLocal)
{
    if (data_)
    {
        delete data_;
        data_ = nullptr;
    }
    
    jitterTiming_.startFramePlayout();
    webrtc::EncodedImage frame;
    
    frameBuffer_->acquireSlot(&data_);
    
    // render frame if we have one
    if (data_ && frameConsumer_)
    {
//        frameno = slot->getFrameNumber();
//        nPlayedOut_++;
//        
//        frameLogger_->log(NdnLoggerLevelInfo,
//                          "PLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld \t%ld \t%.2f \t%d",
//                          slot->getFrameNumber(),
//                          slot->assembledSegmentsNumber(),
//                          slot->totalSegmentsNumber(),
//                          slot->isKeyFrame(),
//                          playoutBuffer_->getJitterSize(),
//                          slot->getAssemblingTimeUsec(),
//                          frame->capture_time_ms_,
//                          currentProducerRate_,
//                          pipelinerBufferSize_);
        ((NdnFrameData*)data_)->getFrame(frame);
        frameConsumer_->onEncodedFrameDelivered(frame);
    }
    
    // get playout time (delay) for the rendered frame
    int framePlayoutTime = frameBuffer_->releaseAcquiredSlot();
    
//    LogTrace("jitter-timing.log")
//    << "playout time " << framePlayoutTime << " data: " << (data_?"YES":"NO") << endl;
//    int adjustedPlayoutTime = 0;
//    // if av sync has been set up
//    if (slot && avSync_.get())
//        adjustedPlayoutTime = avSync_->synchronizePacket(slot, packetTsLocal,
//                                                         this);
    
    if (framePlayoutTime >= 0)
    {
        jitterTiming_.updatePlayoutTime(framePlayoutTime);
        
        // setup and run playout timer for calculated playout interval
        jitterTiming_.runPlayoutTimer();
    }
}