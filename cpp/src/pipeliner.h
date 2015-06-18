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

#include <boost/thread/mutex.hpp>

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
            boost::mutex mutex_;
            std::set<PacketNumber> framePool_;
            const FrameBuffer* frameBuffer_;
        };
        
        
        class PipelinerBase : public NdnRtcComponent,
                                public IFrameBufferCallback,
                                public statistics::StatObject
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
                          const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
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
            
            void
            threadSwitched();
            
            int
            getIdleTime()
            {
                int idleTime = (recoveryCheckpointTimestamp_)?(int)(NdnRtcUtils::millisecondTimestamp() - recoveryCheckpointTimestamp_):0;
                
                ((idleTime > FRAME_DELAY_DEADLINE) ? LogWarnC : LogTraceC)
                << "idle time " << idleTime
                << std::endl;
                
                return idleTime;
            }
            
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
            boost::mutex streamSwitchMutex_;
            bool useKeyNamespace_;
            int64_t recoveryCheckpointTimestamp_, startPhaseTimestamp_;
            
            void
            switchToState(State newState)
            {
                State oldState = state_;
                state_ = newState;
                
                if (oldState != newState)
                {
                    int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
                    int64_t phaseDuration = timestamp - startPhaseTimestamp_;
                    startPhaseTimestamp_ = timestamp;
                    
                    ((oldState == StateChasing) ? LogInfoC : LogDebugC)
                    << "phase " << toString(oldState) << " finished in "
                    << phaseDuration << " msec" << std::endl;
                    
                    LogDebugC << "new state " << toString(state_) << std::endl;
                    
                    if (callback_)
                        callback_->onStateChanged(oldState, state_);
                }
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
            
            // IFrameBufferCallback interface
            virtual void
            onRetransmissionNeeded(FrameBuffer::Slot* slot);
            
            virtual void
            onKeyNeeded(PacketNumber seqNo);
            
            virtual void
            rebuffer() = 0;
            
            virtual void
            resetData();
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
                       const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
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
            
        private:
            StabilityEstimator stabilityEstimator_;
            RttChangeEstimator rttChangeEstimator_;
            PipelinerWindow window_;
            
            uint64_t timestamp_;
            bool waitForChange_, waitForStability_;
            unsigned int failedWindow_;
            FrameNumber seedFrameNo_;
            ndn::Interest rightmostInterest_;
            PacketNumber exclusionPacket_;
            
            // incoming data statistics
            unsigned int dataMeterId_, segmentFreqMeterId_;
            unsigned int nDataReceived_ = 0, nTimeouts_ = 0;
            
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
