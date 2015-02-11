//
//  pipeliner.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__pipeliner__
#define __ndnrtc__pipeliner__

#include "ndnrtc-common.h"
#include "frame-buffer.h"
#include "ndnrtc-object.h"
#include "consumer.h"
#include "playout.h"

namespace ndnrtc {
    namespace new_api {
        
        class PipelinerWindow : public NdnRtcComponent
        {
        public:
            PipelinerWindow();
            ~PipelinerWindow();
            
            void
            init(unsigned int windowSize, const FrameBuffer* frameBuffer);
            
            void
            dataArrived(PacketNumber packetNo);
            
            bool
            canAskForData(PacketNumber packetNo);
            
            unsigned int
            getDefaultWindowSize();
            
            int
            getCurrentWindowSize();
            
            int
            changeWindow(int delta);
            
        private:
            unsigned int dw_;
            int w_;
            PacketNumber lastAddedToPool_;
            webrtc::CriticalSectionWrapper& cs_;
            std::set<PacketNumber> framePool_;
            const FrameBuffer* frameBuffer_;
        };
        
        
        class PipelinerBase : public NdnRtcComponent,
                                public IFrameBufferCallback,
                                public IPlayoutObserver
        {
        public:
            typedef enum _State {
                StateInactive = -1,
                StateWaitInitial = 0,                
                StateChasing = 1,
                StateAdjust = 2,
                StateBuffering = 3,
                StateFetching = 4
            } State;
            
            static const FrameSegmentsInfo DefaultSegmentsInfo;
            
            // average number of segments for delta and key frames
            static const double SegmentsAvgNumDelta;
            static const double SegmentsAvgNumKey;
            static const double ParitySegmentsAvgNumDelta;
            static const double ParitySegmentsAvgNumKey;
            
            PipelinerBase(const boost::shared_ptr<Consumer>& consumer,
                          const FrameSegmentsInfo& frameSegmentsInfo = DefaultSegmentsInfo);
            ~PipelinerBase();
            
            virtual int
            initialize();
            
            virtual int
            start() = 0;
            
            virtual int
            stop() = 0;
            
            void
            setUseKeyNamespace(bool useKeyNamespace)
            { useKeyNamespace_ = useKeyNamespace; }
            
            State
            getState() const
            { return state_; }
            
            void
            registerCallback(IPipelinerCallback* callback)
            { callback_ = callback; }
            
            virtual void
            triggerRebuffering();
            
            virtual PipelinerStatistics
            getStatistics();
            
            void
            threadSwitched();
            
            // IPlayoutObserver interface conformance
            virtual bool
            recoveryCheck() { return false; }
            
            virtual void
            keyFrameConsumed() {}
            
        protected:
            State state_;
            Name threadPrefix_, deltaFramesPrefix_, keyFramesPrefix_;
            
            Consumer* consumer_;
            FrameBuffer* frameBuffer_;
            BufferEstimator* bufferEstimator_;
            IPacketAssembler* ndnAssembler_;
            IPipelinerCallback* callback_;
            
            int deltaSegnumEstimatorId_, keySegnumEstimatorId_;
            int deltaParitySegnumEstimatorId_, keyParitySegnumEstimatorId_;
            unsigned int rtxFreqMeterId_;
            PacketNumber keyFrameSeqNo_, deltaFrameSeqNo_;
            FrameSegmentsInfo frameSegmentsInfo_;
            
            unsigned int streamId_; // currently fetched stream id
            webrtc::CriticalSectionWrapper &streamSwitchSync_;
            PipelinerStatistics stat_;
            bool useKeyNamespace_;
            int64_t recoveryCheckpointTimestamp_;
            
            void
            switchToState(State newState)
            {
                State oldState = state_;
                state_ = newState;
                
                LogDebugC << "new state " << toString(state_) << std::endl;
                
                if (callback_)
                    callback_->onStateChanged(oldState, state_);
            }
            
            std::string
            toString(State state)
            {
                switch (state) {
                    case StateInactive:
                        return "Inactive";
                    case StateChasing:
                        return "Chasing";
                    case StateWaitInitial:
                        return "WaitingInitial";
                    case StateAdjust:
                        return "Adjust";
                    case StateBuffering:
                        return "Buffering";
                    case StateFetching:
                        return "Fetching";
                    default:
                        return "Unknown";
                }
            }
            
            void
            updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                   int nSegments, bool isParity);
            
            void
            requestNextKey(PacketNumber& keyFrameNo);
            
            void
            requestNextDelta(PacketNumber& deltaFrameNo);
            
            void
            prefetchFrame(const Name& basePrefix, PacketNumber packetNo,
                          int prefetchSize, int parityPrefetchSize,
                          FrameBuffer::Slot::Namespace nspc = FrameBuffer::Slot::Delta);
            
            
            void
            expressRange(Interest& interest, SegmentNumber startNo,
                         SegmentNumber endNo, int64_t priority, bool isParity);
            
            void
            express(Interest& interest, int64_t priority);
            
            boost::shared_ptr<Interest>
            getDefaultInterest(const Name& prefix, int64_t timeoutMs = 0);
            
            boost::shared_ptr<Interest>
            getInterestForRightMost(int64_t timeoutMs, bool isKeyNamespace = false,
                                    PacketNumber exclude = -1);
            
            int64_t
            getInterestLifetime(int64_t playbackDeadline,
                                FrameBuffer::Slot::Namespace nspc = FrameBuffer::Slot::Delta,
                                bool rtx = false);
            
            void
            requestMissing(const boost::shared_ptr<FrameBuffer::Slot>& slot,
                           int64_t lifetime, int64_t priority,
                           bool wasTimedOut = false);
            
            virtual void
            onRetransmissionNeeded(FrameBuffer::Slot* slot);
            
            virtual void
            rebuffer() = 0;
            
            virtual void
            resetData();
        };
        
        // chasing pipeliner
        class Pipeliner : public PipelinerBase
        {
        public:
            
            static const int64_t MaxInterruptionDelay;
            static const int64_t MinInterestLifetime;
            static const int MaxRetryNum; // maximum number of retires when
                                          // streaming suddenly inerrupts
            static const int ChaserRateCoefficient; // how faster than producer
                                                    // rate the chaser should
                                                    // skim through cached content
            static const int FullBufferRecycle; // number of slots to recycle if
                                                // the buffer became full during
                                                // chasing stage
            
            Pipeliner(const boost::shared_ptr<Consumer>& consumer,
                      const FrameSegmentsInfo& frameSegmentsInfo = DefaultSegmentsInfo);
            ~Pipeliner();
            
            int
            start();
            
            int
            stop();
            
            PipelinerStatistics
            getStatistics();
            
        private:
            // this events masks are used in different moments during pipeliner
            // and used for filtering buffer events
            // startup events mask is used for pipeline initiation
            static const int StartupEventsMask = FrameBuffer::Event::StateChanged;
            // this events mask is used when rightmost interest has been issued
            // and initial data is awaited
            static const int WaitForRightmostEventsMask = FrameBuffer::Event::FirstSegment | FrameBuffer::Event::Timeout;
            // this events mask is used during chasing stage
            static const int ChasingEventsMask = FrameBuffer::Event::FirstSegment |
                                                 FrameBuffer::Event::Timeout |
                                                 FrameBuffer::Event::StateChanged;
            // this events mask is used during buffering stage
            static const int BufferingEventsMask = FrameBuffer::Event::FirstSegment |
                                                FrameBuffer::Event::Ready |
                                                FrameBuffer::Event::Timeout |
                                                FrameBuffer::Event::StateChanged |
                                                FrameBuffer::Event::NeedKey;
            // this events mask is used during fetching stage
            static const int FetchingEventsMask = BufferingEventsMask |
                                                FrameBuffer::Event::Playout;
            
            
            ChaseEstimation* chaseEstimation_;
            
            webrtc::ThreadWrapper &mainThread_;
            
            bool isPipelinePaused_, isPipelining_;
            int64_t pipelineIntervalMs_;
            webrtc::ThreadWrapper &pipelineThread_;
            webrtc::EventWrapper &pipelineTimer_;
            webrtc::EventWrapper &pipelinerPauseEvent_;
            
            // --
            unsigned int reconnectNum_;
            PacketNumber exclusionFilter_;
            int bufferEventsMask_;
            
            static bool
            mainThreadRoutine(void *pipeliner){
                return ((Pipeliner*)pipeliner)->processEvents();
            }
            bool
            processEvents();
            
            static bool
            pipelineThreadRoutine(void *pipeliner){
                return ((Pipeliner*)pipeliner)->processPipeline();
            }
            bool
            processPipeline();
            
            int
            handleInvalidState(const ndnrtc::new_api::FrameBuffer::Event &event);
            
            int
            handleChase(const FrameBuffer::Event& event);
            
            void
            initialDataArrived(const boost::shared_ptr<FrameBuffer::Slot>& slot);
            
            void
            handleChasing(const FrameBuffer::Event& event);
            
            void
            handleBuffering(const FrameBuffer::Event& event);
            
            int
            handleValidState(const FrameBuffer::Event& event);
            
            void
            handleTimeout(const FrameBuffer::Event& event);
            
            int
            initialize();
            
            void
            startChasePipeliner(PacketNumber startPacketNo,
                                 int64_t intervalMs);
            
            void
            setChasePipelinerPaused(bool isPaused);
            
            void
            stopChasePipeliner();
            
            /**
             * Requests additional frames to keep buffer meet its target size
             * @param useEstimatedSize Indicates whether estimated buffer size 
             * or playable buffer size should be used for checking
             * @return Number of frames requested
             */
            int
            keepBuffer(bool useEstimatedSize = true);
            
            void
            resetData();
            
            bool
            recoveryCheck(const ndnrtc::new_api::FrameBuffer::Event& event);
            
            void
            rebuffer();
        };
        
        // window-based pipeliner
        class Pipeliner2 : public PipelinerBase,
                            public IPacketAssembler,
                            public boost::enable_shared_from_this<Pipeliner2>
        {
        public:
            static const int DefaultWindow;
            static const int DefaultMinWindow;
            
            Pipeliner2(const boost::shared_ptr<Consumer>& consumer,
                       const FrameSegmentsInfo& frameSegmentsInfo = DefaultSegmentsInfo);
            ~Pipeliner2();
            
            int
            start();
            
            int
            stop();
            
            void
            setLogger(ndnlog::new_api::Logger* logger)
            {
                PipelinerBase::setLogger(logger);
                stabilityEstimator_.setLogger(logger);
                rttChangeEstimator_.setLogger(logger);
            }
            
            // IPlayoutObserver interface conformance
            bool
            recoveryCheck();
            
            void
            keyFrameConsumed();
            
            PipelinerStatistics
            getStatistics();
            
        private:
            StabilityEstimator stabilityEstimator_;
            RttChangeEstimator rttChangeEstimator_;
            PipelinerWindow window_;
            
            uint64_t timestamp_;
            bool waitForChange_, waitForStability_;
            unsigned int failedWindow_;
            FrameNumber seedFrameNo_;
            
            void
            askForRightmostData();
            
            void
            askForInitialData(const boost::shared_ptr<Data>& data);
            
            void
            askForSubsequentData(const boost::shared_ptr<Data>& data);
            
            void
            rebuffer();
            
            void
            resetData();
            
            OnData
            getOnDataHandler()
            { return bind(&Pipeliner2::onData, shared_from_this(), _1, _2); }
            
            OnTimeout
            getOnTimeoutHandler()
            { return bind(&Pipeliner2::onTimeout, shared_from_this(), _1); }
            
            void onData(const boost::shared_ptr<const Interest>& interest,
                        const boost::shared_ptr<Data>& data);
            void onTimeout(const boost::shared_ptr<const Interest>& interest);
        };
    }
}

#endif /* defined(__ndnrtc__pipeliner__) */
