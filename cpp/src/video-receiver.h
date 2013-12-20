//
//  video-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_receiver__
#define __ndnrtc__video_receiver__

#include "media-receiver.h"
#include "video-sender.h"
#include "video-playout-buffer.h"

namespace ndnrtc
{
    class NdnVideoReceiver : public NdnMediaReceiver,
    public IPlayoutBufferCallback {
    public:
        NdnVideoReceiver(const ParamsStruct &params);
        ~NdnVideoReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        void setFrameConsumer(IEncodedFrameConsumer *consumer) {
            frameConsumer_ = consumer;
        }
        
        unsigned int getNPipelined() { return pipelinerFrameNo_; }
        unsigned int getJitterOccupancy() { return playoutBuffer_->getJitterSize(); }
        unsigned int getBufferStat(FrameBuffer::Slot::State state) {
            return frameBuffer_.getStat(state);
        }
        
        // IPlayoutBufferCallback interface
        void onFrameAddedToJitter(FrameBuffer::Slot *slot);
        void onBufferStateChanged(PlayoutBuffer::State newState);
        void onMissedFrame(unsigned int frameNo);
        void onPlayheadMoved(unsigned int nextPlaybackFrame);
        void onJitterBufferUnderrun();
        
    private:
        uint64_t playoutLastUpdate_ = 0;
        double publisherRate_ = 0;
        unsigned int emptyJitterCounter_ = 0;
        
        IEncodedFrameConsumer *frameConsumer_;
        webrtc::ThreadWrapper &playoutThread_;
        VideoJitterTiming jitterTiming_;
        
        // static routines for threads
        static bool playoutThreadRoutine(void *obj) {
            return ((NdnVideoReceiver*)obj)->processPlayout();
        }
        
        // thread main functions (called iteratively by static routines)
        bool processPlayout();
        void playbackFrame();
        
        // overriden
        void switchToMode(NdnVideoReceiver::ReceiverMode mode);
        bool isLate(unsigned int frameNo);
        void rebuffer();
        
        unsigned int getNextKeyFrameNo(unsigned int frameNo) DEPRECATED;
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
