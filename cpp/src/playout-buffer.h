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
        shared_ptr<webrtc::EncodedImage> acquireNextFrame();
        void releaseAcquiredFrame();
        
        //        shared_ptr<webrtc::EncodedImage> popFrame();
        
        /**
         * Moves frame pointer forward and frees skipped frames to be re-used by frame buffer
         * @param   frameNo Frame number to wich skipping is done. Frame can be retrieved by calling
         *          popFrame() after this call.
         * @return  0 if everything is ok, negative value if can't skip to the specified frame number
         *          in this case, skipping is done till the latest frame.
         */
        int skipTo(unsigned int frameNo);
        //        int skipToTheLatest();
        
        unsigned int framePointer() { return framePointer_; }
        
    private:
        unsigned int framePointer_;
        FrameBuffer *frameBuffer_;
        // completed frames ready for playout are sorted in ascending order
        std::priority_queue<FrameBuffer::Slot*, std::vector<FrameBuffer::Slot*>, FrameBuffer::Slot::SlotComparator> playoutFrames_;
        
        webrtc::CriticalSectionWrapper &playoutCs_;
        webrtc::ThreadWrapper &providerThread_;
        
        static bool frameProviderThreadRoutine(void *obj) { return ((PlayoutBuffer*)obj)->processFrameProvider(); }
        bool processFrameProvider();
        
        FrameBuffer::Slot *popEarliest();
    };
}

#endif /* defined(__ndnrtc__playout_buffer__) */
