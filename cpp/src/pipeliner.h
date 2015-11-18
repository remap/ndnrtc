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

#include "ndn-assembler.h"
#include "chase-estimation.h"
#include "frame-buffer.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
    namespace new_api {
        
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
        
        class PipelinerWindow : public NdnRtcComponent
        {
        public:
            PipelinerWindow();
            ~PipelinerWindow();
            
            void
            init(unsigned int windowSize, const FrameBuffer* frameBuffer);
            
            void
            reset();
            
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
            
            bool
            isInitialized()
            { return isInitialized_; }
            
        private:
            unsigned int dw_;
            int w_;
            bool isInitialized_;
            PacketNumber lastAddedToPool_;
            boost::mutex mutex_;
            std::set<PacketNumber> framePool_;
            const FrameBuffer* frameBuffer_;
        };
        
        
        class Pipeliner2 : public NdnRtcComponent,
                           public IFrameBufferCallback,
                           public statistics::StatObject,
                           public IPacketAssembler
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
            
            static const int DefaultWindow;
            static const int DefaultMinWindow;
            
            Pipeliner2(const boost::shared_ptr<Consumer>& consumer,
                       const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
                       const FrameSegmentsInfo& frameSegmentsInfo = DefaultSegmentsInfo);
            ~Pipeliner2();
            
            int
            initialize();
            
            int
            start();
            
            int
            stop();
            
            void
            setUseKeyNamespace(bool useKeyNamespace)
            { useKeyNamespace_ = useKeyNamespace; }
            
            State
            getState() const
            { return state_; }
            
            void
            registerCallback(IPipelinerCallback* callback)
            { callback_ = callback; }
            
            void
            triggerRebuffering();
            
            bool
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
            
            void
            setLogger(ndnlog::new_api::Logger* logger)
            {
                NdnRtcComponent::setLogger(logger);
                stabilityEstimator_.setLogger(logger);
                rttChangeEstimator_.setLogger(logger);
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
            bool useKeyNamespace_;
            int64_t recoveryCheckpointTimestamp_, startPhaseTimestamp_;
            std::map<std::string, PacketNumber> deltaSyncList_, keySyncList_;
            
            StabilityEstimator2 stabilityEstimator_;
            RttChangeEstimator rttChangeEstimator_;
            PipelinerWindow window_;
            
            uint64_t timestamp_;
            bool waitForChange_, waitForStability_;
            unsigned int failedWindow_;
            FrameNumber seedFrameNo_;
            ndn::Interest rightmostInterest_;
            PacketNumber exclusionPacket_;
            bool waitForThreadTransition_;
            
            // incoming data statistics
            unsigned int dataMeterId_, segmentFreqMeterId_;
            unsigned int nDataReceived_ = 0, nTimeouts_ = 0;
            
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
                           int64_t lifetime, int64_t priority);
            
            // IFrameBufferCallback interface
            void
            onRetransmissionNeeded(FrameBuffer::Slot* slot);
            
            void
            onKeyNeeded(PacketNumber seqNo);
            
            void
            rebuffer();
            
            void
            resetData();
            
            unsigned int
            getCurrentMinimalLambda();
            
            unsigned int
            getCurrentMaximumLambda();
            
            OnData
            getOnDataHandler()
            { return bind(&Pipeliner2::onData, boost::dynamic_pointer_cast<Pipeliner2>(shared_from_this()), _1, _2); }
            
            OnTimeout
            getOnTimeoutHandler()
            { return bind(&Pipeliner2::onTimeout, boost::dynamic_pointer_cast<Pipeliner2>(shared_from_this()), _1); }
            
            void onData(const boost::shared_ptr<const Interest>& interest,
                        const boost::shared_ptr<Data>& data);
            void onTimeout(const boost::shared_ptr<const Interest>& interest);
            
            void
            onFrameDropped(PacketNumber seguenceNo,
                           PacketNumber playbackNo,
                           FrameBuffer::Slot::Namespace nspc);
            
            void
            askForRightmostData();
            
            void
            askForInitialData(const boost::shared_ptr<Data>& data);
            
            void
            askForSubsequentData(const boost::shared_ptr<Data>& data);
            
            void
            updateThreadSyncList(const PrefixMetaInfo& metaInfo, bool isKey);

            void
            performThreadTransition();
            
        };
    }
}

#endif /* defined(__ndnrtc__pipeliner__) */
