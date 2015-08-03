//
//  consumer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__fetch_channel__
#define __ndnrtc__fetch_channel__

#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/mutex.hpp>

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "interest-queue.h"
#include "chase-estimation.h"
#include "buffer-estimator.h"
#include "statistics.h"
#include "renderer.h"
#include "video-renderer.h"
#include "rate-control.h"

#define SYMBOL_SEG_RATE "sr"
#define SYMBOL_INTEREST_RATE "ir"
#define SYMBOL_PRODUCER_RATE "rate"
#define SYMBOL_JITTER_TARGET "jt"
#define SYMBOL_JITTER_ESTIMATE "je"
#define SYMBOL_JITTER_PLAYABLE "jp"
#define SYMBOL_INRATE "in"
#define SYMBOL_NREBUFFER "nreb"

#define SYMBOL_NPLAYED "npb"
#define SYMBOL_NPLAYEDKEY "npbk"

#define SYMBOL_NSKIPPEDNOKEY "nskipk"
#define SYMBOL_NSKIPPEDINC "nskipi"
#define SYMBOL_NSKIPPEDINCKEY "nskipik"
#define SYMBOL_NSKIPPEDGOP "nskipg"

#define SYMBOL_NACQUIRED "nacq"
#define SYMBOL_NACQUIREDKEY "nacqk"

#define SYMBOL_NDROPPED "ndrop"
#define SYMBOL_NDROPPEDKEY "ndropk"

#define SYMBOL_NASSEMBLED "nasm"
#define SYMBOL_NASSEMBLEDKEY "nasmk"

#define SYMBOL_NRESCUED "nresc"
#define SYMBOL_NRESCUEDKEY "nresck"

#define SYMBOL_NRECOVERED "nrec"
#define SYMBOL_NRECOVEREDKEY "nreck"

#define SYMBOL_NINCOMPLETE "ninc"
#define SYMBOL_NINCOMPLETEKEY "ninck"

#define SYMBOL_NREQUESTED "nreq"
#define SYMBOL_NREQUESTEDKEY "nreqk"

#define SYMBOL_NRTX "nrtx"
#define SYMBOL_AVG_DELTA "ndelta"
#define SYMBOL_AVG_KEY "nkey"
#define SYMBOL_RTT_EST "rtt"
#define SYMBOL_NINTRST "nint"
#define SYMBOL_NDATA "ndata"
#define SYMBOL_NTIMEOUT "nto"
#define SYMBOL_LATENCY "lat"

namespace ndnrtc {
    class AudioVideoSynchronizer;
    
    namespace new_api {
        using namespace ndn;
        
        class FrameBuffer;
        class PipelinerBase;
        class InterestQueue;
        class Playout;
        
        class RttEstimation;
        class ChaseEstimation;
        class BufferEstimator;

        class IPipelinerCallback
        {
        public:
            virtual void
            onBufferingEnded() = 0;
            
            virtual void
            onInitialDataArrived() = 0;
            
            virtual void
            onStateChanged(const int& oldState, const int& newState) = 0;
        };
        
        class ConsumerSettings {
        public:
            std::string userPrefix_;
            MediaStreamParams streamParams_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
        };
        
        /**
         * Consumer class combines all the necessary components for successful
         * fetching media (audio or video) data from the network. Main 
         * components are the follows:
         * - FrameBuffer - used for assembling and ordering packets/frames
         * - PacketPlayback - mechanism for playing packets/frames back
         * - Pipeliner - issues interest and guarantees frame delivery to the 
         *      buffer
         * - InterestQueue - control center for outgoing interests. Outgoing 
         *      interest can be prioritized based on the playback deadlines 
         *      (how many ms left before the deadline)
         * - PacketAssembler - dispatches callbacks from the NDN library, 
         *      appends buffer or notifies about timeout; also, it is responsible
         *      for calling processEvents of the Face object
         * - BufferEtimator - dictates the buffer size based on current 
         *      RttEstimation
         * - RttEstimation - estimates current RTT value
         * - ChaseEstimator - estimates when the pipeliner should switch to the 
         *      Fetch mode
         */
        class Consumer : public NdnRtcComponent,
                         public IPacketAssembler,
                         public IPipelinerCallback
        {
        public:
            
            typedef enum _State {
                StateInactive = -1,
                StateChasing = 0,
                StateFetching = 1
            } State;
            
            static const int MaxIdleTimeMs;
            static const int MaxChasingTimeMs;
            
            Consumer(const GeneralParams& generalParams,
                     const GeneralConsumerParams& consumerParams);
            virtual ~Consumer();
            
            virtual int
            init(const ConsumerSettings& settings,
                 const std::string& threadName);
            
            virtual int
            start();
            
            virtual int
            stop();
            
            void
            registerObserver(IConsumerObserver* const observer)
            {
                boost::lock_guard<boost::mutex> scopedLock(observerMutex_);
                observer_ = observer;
            }

            void
            unregisterObserver()
            {
                boost::lock_guard<boost::mutex> scopedLock(observerMutex_);
                observer_ = NULL;
            }
            
            int
            getIdleTime();
            
            virtual void
            triggerRebuffering();
            
            State
            getState() const;
            
            virtual ConsumerSettings
            getSettings() const
            { return settings_; }
            
            virtual GeneralConsumerParams
            getParameters() const
            { return consumerParams_; }
            
            virtual GeneralParams
            getGeneralParameters() const
            { return generalParams_; }
            
            std::string
            getPrefix() const
            { return streamPrefix_; }
            
            std::string
            getCurrentThreadName() const
            { return settings_.streamParams_.mediaThreads_[currentThreadIdx_]->threadName_; }
            
            MediaThreadParams*
            getCurrentThreadParameters() const
            { return settings_.streamParams_.mediaThreads_[currentThreadIdx_]; }
            
            void
            switchThread(const std::string& threadName);
            
            virtual boost::shared_ptr<FrameBuffer>
            getFrameBuffer() const
            { return frameBuffer_; }
            
            virtual boost::shared_ptr<PipelinerBase>
            getPipeliner() const
            { return pipeliner_; }
            
            virtual boost::shared_ptr<InterestQueue>
            getInterestQueue() const
            { return interestQueue_; }
            
            virtual IPacketAssembler*
            getPacketAssembler();
            
            virtual boost::shared_ptr<Playout>
            getPacketPlayout() const
            { return playout_; }
            
            virtual boost::shared_ptr<RttEstimation>
            getRttEstimation() const
            { return rttEstimation_; }
            
            virtual boost::shared_ptr<ChaseEstimation>
            getChaseEstimation() const
            { return chaseEstimation_; }
            
            virtual boost::shared_ptr<BufferEstimator>
            getBufferEstimator() const
            { return bufferEstimator_; }
            
            virtual boost::shared_ptr<RateControl>
            getRateControlModule() const
            { return rateControl_; }
            
            void
            setRttEstimator(const boost::shared_ptr<RttEstimation>& rttEstimation)
            { rttEstimation_= rttEstimation; }
            
            void
            setInterestQueue(const boost::shared_ptr<InterestQueue>& interestQueue)
            { interestQueue_ = interestQueue; }
            
            void
            setAvSynchronizer(const boost::shared_ptr<AudioVideoSynchronizer>& avSync)
            { avSync_ = avSync; };
            
            boost::shared_ptr<AudioVideoSynchronizer>
            getAvSynchronizer() const
            { return avSync_; }
            
            statistics::StatisticsStorage
            getStatistics() const;
            
            virtual void
            setLogger(ndnlog::new_api::Logger* logger);
            
            virtual void
            setDescription(const std::string& desc);
            
            virtual void
            onBufferingEnded();
            
            virtual void
            onStateChanged(const int& oldState, const int& newState);
            
            virtual void
            onInitialDataArrived();
            
            /**
             * Dumps statistics for the current producer into the log
             * Statistics are dumped in the following format:
             *  - incoming data rate (segments/sec) - "sr"
             *  - interest rate (interests/sec) - "ir"
             *  - producer rate (frames/sec) - "rate"
             *  - jitter buffer target size (ms) - "jt"
             *  - jitter buffer estimate size (ms) - "je"
             *  - jitter buffer playable size (ms) - "jp"
             *  - incoming rate (kbit/sec) - "in"
             *  - number of rebufferings - "nreb"
             *  - number of received frames - "nrecv"
             *  - number of played frames - "npbck"
             *  - number of missed frames - "nmiss"
             *  - number of incomplete frames - "ninc"
             *  - number of rescued frames - "nresc"
             *  - number of recovered frames - "nrec"
             *  - number of retransmissions - "nrtx"
             *  - average number of segments for delta frames - "ndelta"
             *  - average number of segments for key frames - "nkey"
             *  - current rtt estimation - "rtt"
             */
            void
            dumpStat(ReceiverChannelPerformance stat) const;
            
            bool
            getIsConsuming() { return isConsuming_; }
            
        protected:
            bool isConsuming_;
            GeneralParams generalParams_;
            GeneralConsumerParams consumerParams_;
            ConsumerSettings settings_;
            std::string streamPrefix_;
            unsigned int currentThreadIdx_ = 0;
            
            boost::shared_ptr<statistics::StatisticsStorage> statStorage_;
            boost::shared_ptr<FrameBuffer> frameBuffer_;
            boost::shared_ptr<PipelinerBase> pipeliner_;
            boost::shared_ptr<InterestQueue> interestQueue_;
            boost::shared_ptr<Playout> playout_;
            boost::shared_ptr<RttEstimation> rttEstimation_;
            boost::shared_ptr<ChaseEstimation> chaseEstimation_;
            boost::shared_ptr<BufferEstimator> bufferEstimator_;
            boost::shared_ptr<IRenderer> renderer_;
            boost::shared_ptr<AudioVideoSynchronizer> avSync_;
            boost::shared_ptr<RateControl> rateControl_;
            
            boost::mutex observerMutex_;
            IConsumerObserver *observer_;
            
            unsigned int dataMeterId_, segmentFreqMeterId_;
            // statistics
            unsigned int nDataReceived_ = 0, nTimeouts_ = 0;
            
            virtual OnData
            getOnDataHandler()
            { return bind(&Consumer::onData, this, _1, _2); }
            
            virtual OnTimeout
            getOnTimeoutHandler()
            { return bind(&Consumer::onTimeout, this, _1); }
            
            void onData(const boost::shared_ptr<const Interest>& interest,
                        const boost::shared_ptr<Data>& data);
            void onTimeout(const boost::shared_ptr<const Interest>& interest);

        private:
            int
            getThreadIdx(const std::string& threadName);
        };
    }
}

#endif /* defined(__ndnrtc__fetch_channel__) */
