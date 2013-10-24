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
    class NdnVideoReceiver : public NdnMediaReceiver {
    public:
        NdnVideoReceiver(const ParamsStruct &params);
        ~NdnVideoReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        void setFrameConsumer(IEncodedFrameConsumer *consumer) { frameConsumer_ = consumer; }
        
        unsigned int getPlaybackSkipped() { return playbackSkipped_; }
        unsigned int getNPipelined() { return pipelinerFrameNo_; }
        unsigned int getNPlayout() { return playoutFrameNo_; }
        unsigned int getNLateFrames() { return nLateFrames_; }
        unsigned int getBufferStat(FrameBuffer::Slot::State state) { return frameBuffer_.getStat(state); }
        double getPlayoutFreq () { return NdnRtcUtils::currentFrequencyMeterValue(playoutMeterId_); }
        double getIncomeFramesFreq() { return NdnRtcUtils::currentFrequencyMeterValue(incomeFramesMeterId_); }
        double getIncomeDataFreq() { return NdnRtcUtils::currentFrequencyMeterValue(assemblerMeterId_); }
        
    private:
        
        // statistics variables
        unsigned int playoutMeterId_, assemblerMeterId_, incomeFramesMeterId_;
        unsigned int playbackSkipped_ = 0;  // number of packets that were skipped due to late delivery,
                                        // i.e. playout thread requests frames at fixed rate, if a frame
                                        // has not arrived yet (not in playout buffer) - it is skipped
        unsigned int pipelinerOverhead_ = 0;   // number of outstanding frames pipeliner has requested already
        unsigned int nLateFrames_ = 0;      // number of late frames (arrived after their playback time)
        
        bool playout_;
        long playoutSleepIntervalUSec_; // 30 fps
        long playoutFrameNo_;
        
        PlayoutBuffer playoutBuffer_;
        IEncodedFrameConsumer *frameConsumer_;
        
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
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
