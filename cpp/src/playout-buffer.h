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
                 unsigned int gop = INT_MAX,
                 unsigned int jitterSize = 1);
        void flush();
        void registerBufferCallback(IPlayoutBufferCallback *callback) {
            callback_ = callback;
        }
        
        /**
         * Returns next frame to be played
         */
        shared_ptr<webrtc::EncodedImage> acquireNextFrame(bool incCounter = false);
        FrameBuffer::Slot *acquireNextSlot(bool incCounter = false);
        void releaseAcquiredFrame();
        
        int moveTo(unsigned int frameNo){ return 0; }//framePointer_ = frameNo; }
        unsigned int framePointer() DEPRECATED { return framePointer_; }
        unsigned int getCurrentPointer() DEPRECATED { return framePointer_; }
        unsigned int getPlayheadPointer() {
            webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
            return playheadPointer_;
        }
        
        unsigned int getJitterSize() { return jitterBuffer_.size(); };
        void setJitterSize(unsigned int minJitterSize) {
            webrtc::CriticalSectionScoped scopedCs(&syncCs_);
            minJitterSize_ = minJitterSize;
        };
        unsigned int getMinJitterSize() { return minJitterSize_; }
        unsigned int getNKeyFrames() { return nKeyFramesInJitter_; }
        unsigned int getLastKeyFrameNo() { return lastKeyFrameNo_; }
        int getTopFrameNo() {
            return (jitterBuffer_.size())?
                jitterBuffer_.top()->getFrameNumber():-1;
        }
        State getState() { return state_; }
        
    private:
        State state_ = StateUnknown;
        bool isPlaybackStarted_ DEPRECATED;
        int nKeyFramesInJitter_ DEPRECATED,
        lastKeyFrameNo_ DEPRECATED;
        unsigned int framePointer_, playheadPointer_;
        unsigned int minJitterSize_; // number of frames to keep in buffer before
                                       // they can be played out
        unsigned int gopSize_ DEPRECATED;  // number of frames between KEY-frames (group of
                                     // picture)
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
    };
}

#endif /* defined(__ndnrtc__playout_buffer__) */
