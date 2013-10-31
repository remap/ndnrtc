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

namespace ndnrtc
{
    class PlayoutBuffer
    {
    public:
        PlayoutBuffer();
        ~PlayoutBuffer();
        
        
        int init(FrameBuffer *buffer);
        /**
         * Returns next frame to be played
         */
        shared_ptr<webrtc::EncodedImage> acquireNextFrame(bool incCounter = false);
        FrameBuffer::Slot *acquireNextSlot(bool incCounter = false);
        void releaseAcquiredFrame();
        
        int moveTo(unsigned int frameNo){ return framePointer_ = frameNo; }
        unsigned int framePointer() { return framePointer_; }
        
    private:
        unsigned int framePointer_;
        FrameBuffer *frameBuffer_;
        // completed frames ready for playout are sorted in ascending order
        std::priority_queue<FrameBuffer::Slot*, std::vector<FrameBuffer::Slot*>, FrameBuffer::Slot::SlotComparator> playoutFrames_;
        
        webrtc::CriticalSectionWrapper &playoutCs_;
        webrtc::ThreadWrapper &providerThread_;
        
        static bool frameProviderThreadRoutine(void *obj) {
            return ((PlayoutBuffer*)obj)->processFrameProvider();
        }
        bool processFrameProvider();
        
        FrameBuffer::Slot *popEarliest();
    };
}

#endif /* defined(__ndnrtc__playout_buffer__) */
