//
//  playout-buffer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev


#ifndef __ndnrtc__playout_buffer__
#define __ndnrtc__playout_buffer__

#include "ndnrtc-common.h"
#include "frame-buffer.h"
#include "ndnrtc-utils.h"

namespace ndnrtc
{
  typedef std::priority_queue<FrameBuffer::Slot*,
  std::vector<FrameBuffer::Slot*>,
  FrameBuffer::Slot::SlotComparator> FrameJitterBuffer;
  
  const int MinJitterSizeMs = 300;
  const int MaxUnderrunNum = 10;
  const int MaxOutstandingInterests = 1 DEPRECATED;
  
  class IPlayoutBufferCallback;
  
  class PlayoutBuffer
  {
  public:
    enum State {
      StateUnknown = -1,
      StateClear = 0,
      StatePlayback = 1,
      StateBuffering = 2
    };
    
    PlayoutBuffer();
    ~PlayoutBuffer();
    
    int init(FrameBuffer *buffer,
             unsigned int jitterSize = 1);
    void flush();
    void registerBufferCallback(IPlayoutBufferCallback *callback) {
      callback_ = callback;
    }
    
    /**
     * Returns next frame to be played out
     */
    virtual FrameBuffer::Slot *acquireNextSlot(bool incCounter = false);
    /**
     * Releases frame slot which was taken for playout (i.e. it should be
     * called when frame has been already decoded and rendered)
     *
     * @return Playback time for the released frame in milliseconds, i.e.
     * how long the caller should wait before next acquireNextFrame call.
     * This value is calculated based on the next frame in the jitter
     * buffer. In cases, when it is not possible to calculate this value,
     * it returns -1 (when there are no frames in the buffer - underrun, or
     * next frame's number is greater than then acquired frame's number by
     * more than 1 - missed frame).
     */
    virtual int releaseAcquiredFrame();
    
    int moveTo(unsigned int frameNo){ return 0; }
    unsigned int getPlayheadPointer() {
      webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
      return playheadPointer_;
    }
    
    unsigned int getJitterSize() DEPRECATED { return jitterBuffer_.size(); }
    void setMinJitterSize(unsigned int minJitterSize) DEPRECATED {
      webrtc::CriticalSectionScoped scopedCs(&syncCs_);
      minJitterSize_ = minJitterSize;
    }
    unsigned int getMinJitterSize() DEPRECATED { return minJitterSize_; }

    void setMinJitterSizeMs(unsigned int minJitterSizeMs) {
      webrtc::CriticalSectionScoped scopedCs(&syncCs_);
      minJitterSizeMs_ = minJitterSizeMs;
    }
    unsigned int getJitterSizeMs() { return jitterSizeMs_; }
    
    int getTopFrameNo() {
      return (jitterBuffer_.size())?
      jitterBuffer_.top()->getFrameNumber():-1;
    }
    State getState() { return state_; }
    
  protected:
    bool bufferUnderrun_ = false, missingFrame_ = false;
    State state_ = StateUnknown;
    unsigned int framePointer_, playheadPointer_;
    unsigned int minJitterSize_ DEPRECATED; // number of frames to keep in buffer before
                                 // they can be played out
    
    unsigned int minJitterSizeMs_; // minimal jitter size in milliseconds
    unsigned int jitterSizeMs_;  // current size of jitter buffer in
                                 // milliseconds
    
    FrameBuffer *frameBuffer_;
    // completed frames ready for playout are sorted in ascending order
    FrameJitterBuffer jitterBuffer_;
    IPlayoutBufferCallback *callback_ = NULL;
    
    webrtc::CriticalSectionWrapper &playoutCs_, &syncCs_;
    webrtc::ThreadWrapper &providerThread_;
    
    static bool frameProviderThreadRoutine(void *obj) {
      return ((PlayoutBuffer*)obj)->processFrameProvider();
    }
    bool processFrameProvider();
    
    void switchToState(State state);
  };
  
  class IPlayoutBufferCallback {
  public:
    virtual void onFrameAddedToJitter(FrameBuffer::Slot *slot) = 0;
    virtual void onPlayheadMoved(unsigned int nextPlaybackFrame) = 0;
    virtual void onBufferStateChanged(PlayoutBuffer::State newState) = 0;
    virtual void onMissedFrame(unsigned int frameNo) = 0;
    virtual void onJitterBufferUnderrun() = 0;
  };
}

#endif /* defined(__ndnrtc__playout_buffer__) */
