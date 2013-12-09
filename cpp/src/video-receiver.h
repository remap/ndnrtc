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
#include "playout-buffer.h"

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
        
        unsigned int getPlaybackSkipped() { return playbackSkipped_; }
        unsigned int getNPipelined() { return pipelinerFrameNo_; }
        unsigned int getNPlayout() { return playoutFrameNo_; }
        unsigned int getNLateFrames() { return nLateFrames_; }
        unsigned int getJitterOccupancy() { return playoutBuffer_.getJitterSize(); }        
        unsigned int getBufferStat(FrameBuffer::Slot::State state) {
            return frameBuffer_.getStat(state);
        }
        
        // IPlayoutBufferCallback interface
        void onFrameAddedToJitter(FrameBuffer::Slot *slot);
        void onBufferStateChanged(PlayoutBuffer::State newState);
        void onMissedFrame(unsigned int frameNo);
        void onPlayheadMoved(unsigned int nextPlaybackFrame);
        
    private:
        uint64_t playoutLastUpdate_ = 0;
        uint64_t lastFrameTimeMs_ = 0;
        double publisherRate_ = 0;
        int averageProcessingTimeUsec_ = 0;
        
        unsigned int emptyJitterCounter_ DEPRECATED = 0;
        // statistics variables
        unsigned int playbackSkipped_ = 0;  // number of packets that were
                                            // skipped due to late delivery,
                                            // i.e. playout thread requests
                                            // frames at fixed rate, if a frame
                                            // has not arrived yet (not in
                                            // playout buffer) - it is skipped
        unsigned int pipelinerOverhead_ = 0;// number of outstanding frames
                                            // pipeliner has requested already
        unsigned int nLateFrames_ = 0;      // number of late frames (arrived
                                            // after their playback time)
        
        bool playout_ DEPRECATED;
        long playoutSleepIntervalUSec_;     // 30 fps
        long playoutFrameNo_ DEPRECATED;
        
        IEncodedFrameConsumer *frameConsumer_;
        webrtc::EventWrapper &playoutTimer_;
        webrtc::ThreadWrapper &playoutThread_;
        
        // static routines for threads
        static bool playoutThreadRoutine(void *obj) {
            return ((NdnVideoReceiver*)obj)->processPlayout();
        }
        
        // thread main functions (called iteratively by static routines)
        bool processPlayout();
        
        // overriden
        void switchToMode(NdnVideoReceiver::ReceiverMode mode);
        bool isLate(unsigned int frameNo);
        
        unsigned int getNextKeyFrameNo(unsigned int frameNo);
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
