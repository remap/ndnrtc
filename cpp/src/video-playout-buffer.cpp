//
//  video-playout-buffer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 12/9/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "video-playout-buffer.h"

//#define USE_AMP

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
#pragma mark - public
int VideoPlayoutBuffer::init(FrameBuffer *buffer,
                             unsigned int jitterSize,
                             double startFrameRate)
{
    currentPlayoutTimeMs_ = 1000./startFrameRate;
    
    return PlayoutBuffer::init(buffer, jitterSize);
}

shared_ptr<EncodedImage>
VideoPlayoutBuffer::acquireNextFrame(FrameBuffer::Slot **slot,
                                     bool incCounter)
{
    shared_ptr<EncodedImage> frame(nullptr);
    bool frameWasNotPresent = !nextFramePresent_;
    *slot = acquireNextSlot(incCounter);
    
    // frameWasNotPresent - if on the last iteration frame was not in the buffer
    // but now it has been arrived. this basically means, that playout time for
    // the previous frame could not be calculated and it was played out for the
    // same time as a previous frame. therefore, for the newly arrived frame,
    // it's timestamp should reflect this and accomodate previous frame's
    // playout time
    
    if (missingFrame_ || bufferUnderrun_ || frameWasNotPresent)
        lastFrameTimestampMs_ += adaptedPlayoutTimeMs_;
    else
        if (*slot)
        {
            frame = (*slot)->getFrame();

            if (lastFrameTimestampMs_ < frame->capture_time_ms_)
                lastFrameTimestampMs_ =  frame->capture_time_ms_;
            else
                TRACE("[PLAYOUT] frame was missing");
        }
    
    return frame;
}

int VideoPlayoutBuffer::releaseAcquiredFrame()
{
    int res = PlayoutBuffer::releaseAcquiredFrame();
    
    if (jitterBuffer_.size() && res == 0)
    {
        FrameBuffer::Slot *nextSlot = jitterBuffer_.top();
        
        // check if it is a consecutive frame (playheadPointer_ was incremented
        // by releaseAcquiredFrame() call)
        // if not - next frame has not arrived yet - use previous playout time
        if ((nextFramePresent_ = (nextSlot->getFrameNumber() ==
                                  playheadPointer_)))
        {
            uint64_t nextSlotTimestamp = nextSlot->getFrame()->capture_time_ms_;
            currentPlayoutTimeMs_ = (int)((int64_t)nextSlotTimestamp-
                                          (int64_t)lastFrameTimestampMs_);
            
            if (currentPlayoutTimeMs_ < 0)
                currentPlayoutTimeMs_ = 0;
        }
        else
            TRACE("[PLAYOUT] next frame is missing. infer playout time");
    }
    
    // adjust playout time
    adaptedPlayoutTimeMs_ = getAdaptedPlayoutTime(currentPlayoutTimeMs_,
                                                jitterBuffer_.size());
    assert(adaptedPlayoutTimeMs_>=0);
    return adaptedPlayoutTimeMs_;
}

//******************************************************************************
int VideoPlayoutBuffer::getAdaptedPlayoutTime(int playoutTimeMs, int jitterSize)
{
#ifdef USE_AMP
    if (jitterSize < ampThreshold_)
    {
        double jitterSizeDecrease = (double)(ampThreshold_-jitterSize)/(double)ampThreshold_;
        double playouTimeIncrease = jitterSizeDecrease/3.;
        int playoutIncreaseMs = (int)ceil((double)playoutTimeMs*playouTimeIncrease);

        ampExtraTimeMs_ += playoutIncreaseMs;
        playoutTimeMs += playoutIncreaseMs;

        DBG("[PLAYOUT] ADAPTING PLAYOUT TIME: %d (by %.2f%%)",
            playoutTimeMs, playouTimeIncrease*100);
    }
    else
        // if jitter is ok, but we have extra time of delay collected, we
        // should consume it by decreasing playout time
        if (ampExtraTimeMs_)
        {
            int maxDecreasePerFrame = (int)ceil((double)playoutTimeMs*ExtraTimePerFrame);
            
            if (ampExtraTimeMs_ > maxDecreasePerFrame)
            {
                playoutTimeMs -= maxDecreasePerFrame;
                ampExtraTimeMs_ -= maxDecreasePerFrame;                
            }
            else
            {
                playoutTimeMs -= ampExtraTimeMs_;
                ampExtraTimeMs_ = 0;
            }
        }
#endif
    
    return playoutTimeMs;
}
