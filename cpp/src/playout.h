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

#include <boost/asio.hpp>

#include "statistics.h"
#include "ndnrtc-object.h"
#include "jitter-timing.h"

namespace ndnlog {
    namespace new_api {
        class Logger;
    }
}

namespace ndnrtc {
    class BufferSlot;
    class PlaybackQueue;
    typedef new_api::statistics::StatisticsStorage StatStorage; 
    class IPlayoutObserver;

    class Playout : public NdnRtcComponent, 
                    public new_api::statistics::StatObject
    {
    public:
        Playout(boost::asio::io_service& io,
            const boost::shared_ptr<PlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage> statStorage = 
                boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));
        ~Playout();

        virtual void start(unsigned int fastForwardMs = 0);
        virtual void stop();

        void setLogger(ndnlog::new_api::Logger* logger);
        void setDescription(const std::string& desc);
        bool isRunning() { return isRunning_; }

        void attach(IPlayoutObserver* observer);
        void detach(IPlayoutObserver* observer);

    protected:
        Playout(const Playout&) = delete;

        mutable boost::recursive_mutex mutex_;
        boost::atomic<bool> isRunning_;
        boost::shared_ptr<PlaybackQueue> pqueue_;
        JitterTiming jitterTiming_;
        int64_t lastTimestamp_, lastDelay_, delayAdjustment_;
        std::vector<IPlayoutObserver*> observers_;

        void extractSample();
        virtual void processSample(const boost::shared_ptr<const BufferSlot>&) = 0;
        
        void correctAdjustment(int64_t newSampleTimestamp);
        int64_t adjustDelay(int64_t delay);
    };

    class IPlayoutObserver
    {
    public:
        virtual void onQueueEmpty() = 0;
    };

    #if 0
    class PacketData;
    class JitterTiming;
    
    namespace new_api {
        class Consumer;
        class FrameBuffer;
        
        /**
         * Base class for playout mechanisms. The core playout logic is similar 
         * for audio and video streams. Differences must be implemented in 
         * overriden method playbackPacket which is called from main playout 
         * routine each time playout timer fires. Necessary information is 
         * provided as arguments to the method.
         */
        class Playout : public NdnRtcComponent, public statistics::StatObject
        {
        public:
            Playout(Consumer* consumer,
                    const boost::shared_ptr<statistics::StatisticsStorage>& statStorage);
            virtual ~Playout();
            
            virtual int
            init(void* frameConsumer);
            
            virtual int
            start(int initialAdjustment = 0);
            
            virtual int
            stop();
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
            void
            setDescription(const std::string& desc);
            
            bool
            isRunning()
            { return isRunning_; }
            
        protected:
            bool isRunning_;
            
            bool isInferredPlayback_;
            int64_t lastPacketTs_;
            unsigned int inferredDelay_;
            
            Consumer* consumer_;
            boost::shared_ptr<FrameBuffer> frameBuffer_;
            
            boost::shared_ptr<JitterTiming> jitterTiming_;
            boost::mutex playoutMutex_;
            
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
            
            void
            updatePlaybackAdjustment();
            
            int
            playbackDelayAdjustment(int playbackDelay);
            
            int
            avSyncAdjustment(int64_t nowTimestamp, int playbackDelay);
            
            bool
            processPlayout();
        };
    }
    #endif
}

#endif /* defined(__ndnrtc__playout__) */
