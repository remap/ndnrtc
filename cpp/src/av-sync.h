//
//  av-sync.h
//  ndnrtc
//
//  Created by Peter Gusev on 1/17/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__av_sync__
#define __ndnrtc__av_sync__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"

namespace ndnrtc {
    
    class AudioVideoSynchronizer : public LoggerObject
    {
    public:
        AudioVideoSynchronizer(double audioRate, double videoRate);
        ~AudioVideoSynchronizer(){}
        
        /**
         * Sets synchronization frequency. This value will be used for 
         * calculating main synchronization points - frame numbers at which 
         * synchronization should be performed.
         * @param syncFrequency Desired synchronization frequency (times per 
         * second).
         */
        void setSynchronizationFrequency(double syncFrequency);
        
        /**
         * Main method which should be called for synchronization. It should
         * be called once the packet was released from the playout buffer (i.e. 
         * releaseAcquiredSlot was called) BUT right before running playout 
         * timer (i.e. runPlayoutTimer call of JitterTiming class). Upon 
         * return, it provides delay by which playout time should be 
         * incremented in order to be synchronized with the other stream.
         * @param slot Slot which contains packet to be played out. By its 
         * content (audio sample or video frame) it also determines whether 
         * audio or video stream should be synchronized.
         * @param playoutDuration Playout duration returned by playout buffer.
         * @return Playout duration adjustment (in ms) needed for media streams 
         * to be synchronized or 0 if no adjustment is required.
         */
        int synchronizePacket(FrameBuffer::Slot* slot,
                              int playoutDuration);
        
        /**
         * Updates actual audio packet rate
         * @param audioPacketRate Actual packet rate for audio stream
         */
        void updateAudioPacketRate(double audioPacketRate)
        {
            updateRate(audioSyncData_, audioPacketRate);
        }
        
        /**
         * Updates actual video packet rate
         * @param videoPacketRate Actual packet rate for video stream
         */
        void updateVideoPacketRate(double videoPacketRate)
        {
            updateRate(videoSyncData_, videoPacketRate);
        }
        
    private:
        // those attribute which has prefix "remote" relate to remote producer's
        // timestamps, wherease suffixed with "local" relate to local timestamps
        typedef struct _SyncStruct {
            int lastPacketNumber_;  // last packet number
            int64_t playbackStartLocal_ = -1;
            int64_t playbackStartRemote_ = -1;   // playback start
            int64_t lastPacketTsLocal_; // local timestamp of last packet
                                        // playout time, i.e. when packet was
                                        // acquired for playback
            int64_t lastPacketTsRemote_;    // remote timestamp of last packet
                                            // playout time, i.e. packet
                                            // timestamp published by producer
            int64_t lastPacketPlayoutDuration_; // calculated playout duration
                                                // of last packet (including
                                                // AMP adjustments)
            double actualPacketRate_;   // most recent value of packet rate
                                        // provided by producer
            int nextSyncPoint_; // next synchronization point, i.e. number of
                                // the packet which should be analyzed for
                                // AV synchronizations
        } SyncStruct;
        
        double syncFrequency_; // how often synchronization should be invoked
                               // (per second, i.e. value of 2 means
                               // synchronization mechanism will run twice per
                               // second)
        SyncStruct audioSyncData_, videoSyncData_;
        
        int syncPacket(SyncStruct& syncData, FrameBuffer::Slot* slot,
                       int playoutDuration);
        void updateRate(SyncStruct& syncData, double rate);
        void initialize(SyncStruct& syncData, FrameBuffer::Slot* slot,
                        int playoutDuration);
    };
}

#endif /* defined(__ndnrtc__av_sync__) */
