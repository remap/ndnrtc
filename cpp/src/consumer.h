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

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "interest-queue.h"
#include "chase-estimation.h"
#include "buffer-estimator.h"
#include "statistics.h"
#include "renderer.h"
#include "rate-control.h"
#include "service-channel.h"

#define SYMBOL_SEG_RATE "sr"
#define SYMBOL_INTEREST_RATE "ir"
#define SYMBOL_PRODUCER_RATE "rate"
#define SYMBOL_JITTER_TARGET "jt"
#define SYMBOL_JITTER_ESTIMATE "je"
#define SYMBOL_JITTER_PLAYABLE "jp"
#define SYMBOL_INRATE "in"
#define SYMBOL_NREBUFFER "nreb"
#define SYMBOL_NRECEIVED "nrecv"
#define SYMBOL_NPLAYED "npbck"
#define SYMBOL_NMISSED "nmiss"
#define SYMBOL_NINCOMPLETE "ninc"
#define SYMBOL_NRESCUED "nresc"
#define SYMBOL_NRECOVERED "nrec"
#define SYMBOL_NRTX "nrtx"
#define SYMBOL_AVG_DELTA "ndelta"
#define SYMBOL_AVG_KEY "nkey"
#define SYMBOL_RTT_EST "rtt"

namespace ndnrtc {
    class AudioVideoSynchronizer;
    
    namespace new_api {
        using namespace ndn;
        using namespace ptr_lib;
        
        class FrameBuffer;
        class Pipeliner;
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
            onRebufferingOccurred() = 0;
            
            virtual void
            onStateChanged(const int& oldState, const int& newState) = 0;
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
        class Consumer : public NdnRtcObject,
                         public IPacketAssembler,
                         public IPipelinerCallback,
                         public boost::enable_shared_from_this<Consumer>
        {
        public:
            
            typedef enum _State {
                StateInactive = -1,
                StateChasing = 0,
                StateFetching = 1
            } State;
            
            Consumer(const ParamsStruct& params,
                     const shared_ptr<InterestQueue>& interestQueue,
                     const shared_ptr<RttEstimation>& rttEstimation = shared_ptr<RttEstimation>(nullptr));
            virtual ~Consumer();
            
            virtual int
            init();
            
            virtual int
            start();
            
            virtual int
            stop();
            
            virtual void
            reset();
            
            void
            triggerRebuffering();
            
            State
            getState() const;
            
            virtual ParamsStruct
            getParameters() const
            { return params_; }
            
            virtual shared_ptr<FrameBuffer>
            getFrameBuffer() const
            { return frameBuffer_; }
            
            virtual shared_ptr<Pipeliner>
            getPipeliner() const
            { return pipeliner_; }
            
            virtual shared_ptr<InterestQueue>
            getInterestQueue() const
            { return interestQueue_; }
            
            virtual IPacketAssembler*
            getPacketAssembler()
            { return this; }
            
            virtual shared_ptr<Playout>
            getPacketPlayout() const
            { return playout_; }
            
            virtual shared_ptr<RttEstimation>
            getRttEstimation() const
            { return rttEstimation_; }
            
            virtual shared_ptr<ChaseEstimation>
            getChaseEstimation() const
            { return chaseEstimation_; }
            
            virtual shared_ptr<BufferEstimator>
            getBufferEstimator() const
            { return bufferEstimator_; }
            
            virtual shared_ptr<RateControl>
            getRateControlModule() const
            { return rateControl_; }
            
            void
            setRttEstimator(const shared_ptr<RttEstimation>& rttEstimation)
            { rttEstimation_= rttEstimation; }
            
            void
            setInterestQueue(const shared_ptr<InterestQueue>& interestQueue)
            { interestQueue_ = interestQueue; }
            
            void
            setAvSynchronizer(const shared_ptr<AudioVideoSynchronizer>& avSync)
            { avSync_ = avSync; };
            
            shared_ptr<AudioVideoSynchronizer>
            getAvSynchronizer() const
            { return avSync_; }
            
            void
            getStatistics(ReceiverChannelPerformance& stat) const;
            
            virtual void
            setLogger(ndnlog::new_api::Logger* logger);
            
            virtual void
            setDescription(const std::string& desc);
            
            virtual void
            onBufferingEnded();
            
            virtual void
            onRebufferingOccurred();
            
            virtual void
            onStateChanged(const int& oldState, const int& newState);
            
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
            dumpStat(const std::string& comment = "") const;
            
        protected:
            bool isConsuming_;
            
            shared_ptr<FrameBuffer> frameBuffer_;
            shared_ptr<Pipeliner> pipeliner_;
            shared_ptr<InterestQueue> interestQueue_;
            shared_ptr<Playout> playout_;
            shared_ptr<RttEstimation> rttEstimation_;
            shared_ptr<ChaseEstimation> chaseEstimation_;
            shared_ptr<BufferEstimator> bufferEstimator_;
            shared_ptr<IRenderer> renderer_;
            shared_ptr<AudioVideoSynchronizer> avSync_;
            shared_ptr<RateControl> rateControl_;
            shared_ptr<ServiceChannel> serviceChannel_;
            
            unsigned int dataMeterId_, segmentFreqMeterId_;
            
            virtual OnData
            getOnDataHandler()
            { return bind(&Consumer::onData, this, _1, _2); }
            
            virtual OnTimeout
            getOnTimeoutHandler()
            { return bind(&Consumer::onTimeout, this, _1); }
            
            void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data);
            void onTimeout(const shared_ptr<const Interest>& interest);
        };
    }
}

#endif /* defined(__ndnrtc__fetch_channel__) */
