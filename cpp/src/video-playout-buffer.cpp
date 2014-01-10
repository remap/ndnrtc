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
#pragma mark - construction/destruction
VideoJitterTiming::VideoJitterTiming():
playoutTimer_(*EventWrapper::Create())
{
  
}

//******************************************************************************
#pragma mark - public
void VideoJitterTiming::flush()
{
  framePlayoutTimeMs_ = 0;
  processingTimeUsec_ = 0;
  playoutTimestampUsec_ = 0;
  //    stop();
}
void VideoJitterTiming::stop()
{
  playoutTimer_.StopTimer();
  playoutTimer_.Set();
}

int64_t VideoJitterTiming::startFramePlayout()
{
  int64_t processingStart = NdnRtcUtils::microsecondTimestamp();
  TRACE("PROCESSING START: %ld", processingStart);
  
  if (playoutTimestampUsec_ == 0)
    playoutTimestampUsec_ = NdnRtcUtils::microsecondTimestamp();
  else
  { // calculate processing delay from the previous iteration
    int64_t prevIterationProcTimeUsec = processingStart -
    playoutTimestampUsec_;
    
    TRACE("[PLAYOUT] prev iteration time %ld", prevIterationProcTimeUsec);
    
    // substract frame playout delay
    if (prevIterationProcTimeUsec >= framePlayoutTimeMs_*1000)
      prevIterationProcTimeUsec -= framePlayoutTimeMs_*1000;
    else
      // should not occur!
      assert(0);
    
    TRACE("[PLAYOU] prev iter proc time %ld", prevIterationProcTimeUsec);
    
    // add this time to the average processing time
    processingTimeUsec_ += prevIterationProcTimeUsec;
    TRACE("[PLAYOU] proc time %ld", prevIterationProcTimeUsec);
    
    playoutTimestampUsec_ = processingStart;
  }
  
  return playoutTimestampUsec_;
}

void VideoJitterTiming::updatePlayoutTime(int framePlayoutTime)
{
  TRACE("producer playout time %ld", framePlayoutTime);
  
  int playoutTimeUsec = framePlayoutTime*1000;
  if (playoutTimeUsec < 0) playoutTimeUsec = 0;
  
  if (processingTimeUsec_ >= 1000)
  {
    TRACE("[PLAYOUT] accomodate processing time %ld",
          processingTimeUsec_);
    
    int processingUsec = (processingTimeUsec_/1000)*1000;
    
    TRACE("[PLAYOUT] processing %d", processingUsec);
    
    if (processingUsec > playoutTimeUsec)
    {
      TRACE("[PLAYOUT] skipping frame. processing %d, playout %d",
            processingUsec, playoutTimeUsec);
      
      processingUsec = playoutTimeUsec;
      playoutTimeUsec = 0;
    }
    else
      playoutTimeUsec -= processingUsec;
    
    processingTimeUsec_ = processingTimeUsec_ - processingUsec;
    TRACE("[PLAYOUT] playout usec %d, processing %ld",
          playoutTimeUsec, processingTimeUsec_);
  }
  
  framePlayoutTimeMs_ = playoutTimeUsec/1000;
}

void VideoJitterTiming::runPlayoutTimer()
{
  assert(framePlayoutTimeMs_ >= 0);
  if (framePlayoutTimeMs_ > 0)
  {
    TRACE("TIMER WAIT %d", framePlayoutTimeMs_);
    playoutTimer_.StartTimer(false, framePlayoutTimeMs_);
    playoutTimer_.Wait(WEBRTC_EVENT_INFINITE);
    TRACE("TIMER DONE");
  }
  else
    TRACE("playout time zero - skipping frame");
}

//******************************************************************************
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
      TRACE("[PLAYOUT] next frame %d, %ld",
            nextSlot->getFrameNumber(), nextSlotTimestamp);
      
      currentPlayoutTimeMs_ = (int)((int64_t)nextSlotTimestamp-
                                    (int64_t)lastFrameTimestampMs_);
      
      TRACE("[PLAYOUT] got playout for %d - %d",
            playheadPointer_-1, currentPlayoutTimeMs_);
      
      if (currentPlayoutTimeMs_ < 0)
        currentPlayoutTimeMs_ = 0;
    }
    else
      TRACE("[PLAYOUT] next frame is missing. infer playout time %d",
            adaptedPlayoutTimeMs_);
  }
  else
    TRACE("[PLAYOUT] nothing to release - %d", adaptedPlayoutTimeMs_);
  
  // adjust playout time
  adaptedPlayoutTimeMs_ = getAdaptedPlayoutTime(currentPlayoutTimeMs_,
                                                jitterBuffer_.size());
  TRACE("[PLAYOUT] adapted time %d", adaptedPlayoutTimeMs_);
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
