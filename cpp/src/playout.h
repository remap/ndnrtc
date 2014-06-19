//
//  playout.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#ifndef __ndnrtc__playout__
#define __ndnrtc__playout__

#include "ndnrtc-common.h"
#include "jitter-timing.h"
#include "consumer.h"
#include "frame-buffer.h"
#include "video-sender.h"

namespace ndnrtc{
    namespace new_api {
        
        /**
         * Base class for playout mechanisms. The core playout logic is similar 
         * for audio and video streams. Differences must be implemented in 
         * overriden method playbackPacket which is called from main playout 
         * routine each time playout timer fires. Necessary information is 
         * provided as arguments to the method.
         */
        class Playout : public NdnRtcObject
        {
        public:
            Playout(const Consumer* consumer);
            virtual ~Playout();
            
            virtual int
            init(void* frameConsumer);
            
            virtual int
            start();
            
            virtual int
            stop();
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
            void
            setDescription(const std::string& desc);
            
            void
            getStatistics(ReceiverChannelPerformance& stat);
            
            void
            setStartPacketNo(PacketNumber packetNo)
            { startPacketNo_ = packetNo; }
            
            bool
            isRunning()
            { return isRunning_; }
            
        protected:
            bool isRunning_;
            unsigned int nPlayed_, nMissed_;
            double latency_;
            
            bool isInferredPlayback_;
            int64_t lastPacketTs_;
            unsigned int inferredDelay_;
            int playbackAdjustment_;
            PacketNumber startPacketNo_;
            
#if 1 // left for future testing
            int test_timelineDiff_ = -1;
            int test_timelineDiffInclineEst_ = -1;
#endif
            
            const Consumer* consumer_;
            shared_ptr<FrameBuffer> frameBuffer_;
            
            JitterTiming jitterTiming_;
            webrtc::ThreadWrapper &playoutThread_;
            
            void* frameConsumer_;
            PacketData *data_;
            
            /**
             * This method should be overriden by derived classes for 
             * media-specific playback (audio/video)
             * @param packetTsLocal Packet local timestamp
             * @param data Packet data retrieved from the buffer
             * @param packetNo Packet playback number provided by a producer
             * @param isKey Indicates, whether the packet is a key frame (@note 
             * always false for audio packets)
             * @param assembledLevel Ratio indicating assembled level of the 
             * packet: number of fetched segments divided by total number of 
             * segments for this packet
             */
            virtual bool
            playbackPacket(int64_t packetTsLocal, PacketData* data,
                           PacketNumber playbackPacketNo,
                           PacketNumber sequencePacketNo,
                           PacketNumber pairedPacketNo,
                           bool isKey, double assembledLevel) = 0;
            
            static bool
            playoutThreadRoutine(void *obj)
            {
                return ((Playout*)obj)->processPlayout();
            }
            
            void
            updatePlaybackAdjustment();
            
            void
            adjustPlaybackDelay(int& playbackDelay);
            
            int
            avSyncAdjustment(int64_t nowTimestamp, int playbackDelay);
            
            bool
            processPlayout();
        };
    }
}

#endif /* defined(__ndnrtc__playout__) */
