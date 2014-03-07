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
#include "fetch-channel.h"

namespace ndnrtc {
    namespace new_api {
        class Pipeliner : NdnRtcObject
        {
        public:
            Pipeliner(const shared_ptr<const FetchChannel> &fetchChannel);
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
            
            shared_ptr<const FetchChannel> fetchChannel_;
            ParamsStruct params_; 
            shared_ptr<ndnrtc::new_api::FrameBuffer> frameBuffer_;
            shared_ptr<ChaseEstimation> chaseEstimation_;
            shared_ptr<BufferEstimator> bufferEstimator_;
            
            int defaultInterestTimeoutMs_;
            
            bool isProcessing_;
            webrtc::ThreadWrapper &mainThread_;
            webrtc::ThreadWrapper &pipelineThread_;
            webrtc::EventWrapper &pipelineTimer_;
            bool isPipelining_;
            int64_t pipelineIntervalMs_;
            
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
            
            int
            handleValidState(const FrameBuffer::Event& event);
            
            int
            initialize();
            
            shared_ptr<Interest>
            getDefaultInterest(const Name& prefix);
            
            shared_ptr<Interest>
            getInterestForRightMost(int timeout, bool isKeyNamespace = false,
                                    PacketNumber exclude = -1);
            
            void
            updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                   int nSegments);
            
            void
            requestKeyFrame(PacketNumber keyFrameNo);
            
            void
            expressRange(Interest& interest, SegmentNumber startNo,
                         SegmentNumber endNo, int64_t priority = 0);
            
            void
            express(const Interest& interest, int64_t priority = 0);
            
            void
            startPipeliningDelta(PacketNumber startPacketNo,
                                 int64_t intervalMs);
            
            void
            stopPipeliningDelta();
            
            void
            requestMissing(const shared_ptr<FrameBuffer::Slot>& slot,
                           int64_t lifetime, int64_t priority);
        };
    }
}

#endif /* defined(__ndnrtc__pipeliner__) */
