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

namespace ndnrtc {
    namespace new_api {
        class Pipeliner : NdnRtcObject
        {
        public:
            // average number of segments for delta and key frames
            static const double SegmentsAvgNumDelta;
            static const double SegmentsAvgNumKey;
            
            Pipeliner(const shared_ptr<const Consumer> &consumer);
            ~Pipeliner();
            
            int start();
            int stop();
            
            // ILoggingObject conformance
            virtual std::string
            getDescription()
            {
                return string("pipeliner");
            }
            
        private:
            Name streamPrefix_, deltaFramesPrefix_, keyFramesPrefix_;
            
            shared_ptr<const Consumer> consumer_;
            ParamsStruct params_; 
            shared_ptr<ndnrtc::new_api::FrameBuffer> frameBuffer_;
            shared_ptr<ChaseEstimation> chaseEstimation_;
            shared_ptr<BufferEstimator> bufferEstimator_;
            shared_ptr<IPacketAssembler> ndnAssembler_;
            
            bool isProcessing_;
            webrtc::ThreadWrapper &mainThread_;
            
            bool isPipelining_, isPipelinePaused_, isBuffering_;
            int64_t pipelineIntervalMs_;
            webrtc::ThreadWrapper &pipelineThread_;
            webrtc::EventWrapper &pipelineTimer_;
            webrtc::EventWrapper &pipelinerPauseEvent_;
            
            int deltaSegnumEstimatorId_, keySegnumEstimatorId_;
            PacketNumber keyFrameSeqNo_, deltaFrameSeqNo_;
            
            
            // --
            int
            bufferEventsMask_;
            
            static bool
            mainThreadRoutine(void *pipeliner){
                return ((Pipeliner*)pipeliner)->processEvents();
            }
            bool
            processEvents();
            
            static bool
            pipelineThreadRoutin(void *pipeliner){
                return ((Pipeliner*)pipeliner)->processPipeline();
            }
            bool
            processPipeline();
            
            int
            scanBuffer();
            
            int
            handleInvalidState(const ndnrtc::new_api::FrameBuffer::Event &event);
            
            int
            handleChase(const FrameBuffer::Event& event);
            
            void
            initialDataArrived(const shared_ptr<FrameBuffer::Slot>& slot);
            
            void
            chaseDataArrived(const FrameBuffer::Event& event);
            
            void
            handleBuffering(const FrameBuffer::Event& event);
            
            int
            handleValidState(const FrameBuffer::Event& event);
            
            void
            handleTimeout(const FrameBuffer::Event& event);
            
            int
            initialize();
            
            shared_ptr<Interest>
            getDefaultInterest(const Name& prefix, int64_t timeoutMs = 0);
            
            shared_ptr<Interest>
            getInterestForRightMost(int64_t timeoutMs, bool isKeyNamespace = false,
                                    PacketNumber exclude = -1);
            
            void
            updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                   int nSegments);
            
            void
            requestKeyFrame(PacketNumber keyFrameNo);
            
            void
            requestDeltaFrame(PacketNumber deltaFrameNo);
            
            void
            expressRange(Interest& interest, SegmentNumber startNo,
                         SegmentNumber endNo, int64_t priority = 0);
            
            void
            express(Interest& interest, int64_t priority = 0);
            
            void
            startChasePipeliner(PacketNumber startPacketNo,
                                 int64_t intervalMs);
            
            void
            setChasePipelinerPaused(bool isPaused);
            
            void
            stopChasePipeliner();
            
            void
            requestMissing(const shared_ptr<FrameBuffer::Slot>& slot,
                           int64_t lifetime, int64_t priority);
            
            int64_t
            getInterestLifetime(FrameBuffer::Slot::Namespace nspc = FrameBuffer::Slot::Delta);
            
            void
            prefetchFrame(const Name& basePrefix, PacketNumber packetNo,
                          int prefetchSize);
            
            void
            keepBuffer(bool useEstimatedSize = true);
        };
    }
}

#endif /* defined(__ndnrtc__pipeliner__) */
