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
    
    const int64_t TolerableDriftMs = 20; // packets will be synchronized
                                         // if their timelines differ more
                                         // than this value
    class NdnMediaReceiver;
    
    class IMediaReceiverCallback
    {
    public:
        // called whenever receiver encounters rebuffering
        virtual void onRebuffer(NdnMediaReceiver *caller) = 0;
    };
    
    class AudioVideoSynchronizer : public LoggerObject,
    public IMediaReceiverCallback
    {
    public:
        AudioVideoSynchronizer(ParamsStruct videoParams, ParamsStruct audioParams);
        ~AudioVideoSynchronizer(){}
        
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
        int synchronizePacket(FrameBuffer::Slot* slot, int64_t localTimestamp,
                              NdnMediaReceiver *receiver);
        
        /**
         * Resets synchronization data (used for rebufferings).
         */
        void reset();
        
        /**
         * IMediaReceiverCallback interface
         */
        void onRebuffer(NdnMediaReceiver *receiver);
        
    protected:
        // those attribute which has prefix "remote" relate to remote producer's
        // timestamps, wherease suffixed with "local" relate to local timestamps
        class SyncStruct {
        public:
            SyncStruct(const char *name):
            cs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
            name_(name),
            initialized_(false),
            lastPacketTsLocal_(-1),
            lastPacketTsRemote_(-1)
            {}
            
            webrtc::CriticalSectionWrapper &cs_;
            NdnMediaReceiver *receiver_;
            const char *name_;
            bool initialized_;
            
            int64_t lastPacketTsLocal_; // local timestamp of last packet
                                        // playout time, i.e. when packet was
                                        // acquired for playback
            int64_t lastPacketTsRemote_;    // remote timestamp of last packet
                                            // playout time, i.e. packet
                                            // timestamp published by producer
            
            // resets sync data strcuture
            void reset(){
                webrtc::CriticalSectionScoped scopedCs(&cs_);
                initialized_ = false;
                lastPacketTsLocal_ = -1;
                lastPacketTsRemote_ = -1;
            }
        };
        
        bool initialized_;  // indicates, whether synchronizer was initialized
                       // (when both streams has started)
        webrtc::CriticalSectionWrapper &syncCs_;
        SyncStruct audioSyncData_, videoSyncData_;
        ParamsStruct videoParams_, audioParams_;
        
        int syncPacket(SyncStruct& syncData,
                       SyncStruct& pairedSyncData,
                       int64_t packetTsRemote,
                       int64_t packetTsLocal,
                       NdnMediaReceiver *receiver);
        
        void initialize(SyncStruct& syncData,
                        int64_t firstPacketTsRemote,
                        int64_t packetTsLocal,
                        NdnMediaReceiver *receiver);
        
        int64_t getNextSyncPoint(int64_t startingPoint);
    };
}

#endif /* defined(__ndnrtc__av_sync__) */
