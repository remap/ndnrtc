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

#include "params.h"
#include "ndnrtc-object.h"
#include "face-wrapper.h"
#include "pipeliner.h"

namespace ndnrtc {
    class AudioVideoSynchronizer;
    
    namespace new_api {
        namespace statistics
        {
            class StatisticsStorage;
        }
        
        class IPacketAssembler;
        class IRenderer;
        class FrameBuffer;
        class Pipeliner2;
        class InterestQueue;
        class Playout;
        class RttEstimation;
        class ChaseEstimation;
        class BufferEstimator;
        
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
            
            virtual boost::shared_ptr<Pipeliner2>
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
            
            bool
            getIsConsuming() { return isConsuming_; }
            
        protected:
            bool isConsuming_;
            GeneralParams generalParams_;
            GeneralConsumerParams consumerParams_;
            ConsumerSettings settings_;
            std::string streamPrefix_;
            int currentThreadIdx_ = 0;
            
            boost::shared_ptr<statistics::StatisticsStorage> statStorage_;
            boost::shared_ptr<FrameBuffer> frameBuffer_;
            boost::shared_ptr<Pipeliner2> pipeliner_;
            boost::shared_ptr<InterestQueue> interestQueue_;
            boost::shared_ptr<Playout> playout_;
            boost::shared_ptr<RttEstimation> rttEstimation_;
            boost::shared_ptr<ChaseEstimation> chaseEstimation_;
            boost::shared_ptr<BufferEstimator> bufferEstimator_;
            boost::shared_ptr<IRenderer> renderer_;
            boost::shared_ptr<AudioVideoSynchronizer> avSync_;
            
            boost::mutex observerMutex_;
            IConsumerObserver *observer_;
            
        private:
            int
            getThreadIdx(const std::string& threadName);
        };
    }
}

#endif /* defined(__ndnrtc__fetch_channel__) */
