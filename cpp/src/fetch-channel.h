//
//  fetch-channel.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__fetch_channel__
#define __ndnrtc__fetch_channel__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "interest-queue.h"
#include "chase-estimator.h"
#include "buffer-estimator.h"

namespace ndnrtc {
    namespace new_api {
        
        class FrameBuffer;
        class Pipeliner;
        class InterestQueue;
        class IPacketAssembler;
        class PacketPlayback;
        
        class RttEstimation;
        class ChaseEstimation;
        class BufferEstimator;
        
        /**
         * Fetch channel combines all the necessary components for successful 
         * fetching media (audio or video) data from the network. Main 
         * components are the follows:
         * - FrameBuffer - used for assembling and ordering packets/frames
         * - PacketPlayback - mechanism for playing packets/frames back
         * - Pipeliner - issues interest and guarantees frame delivery to the 
         *      buffer
         * - InterestQueue - control center for outgoing interests. Outgoing 
         *      interest can be prioritized based on the playback deadlines 
         *      (how many ms left for the deadline)
         * - PacketAssembler - dispatches callbacks from the NDN library, 
         *      appends buffer or notifies about timeout; also, is responsible 
         *      for calling processEvents of the Face object
         * - BufferEtimator - dictates the buffer size based on current 
         *      RttEstimation
         * - RttEstimation - estimates current RTT value
         * - ChaseEstimator - estimates when the pipeliner should switch to the 
         *      Fetch mode
         */
        class FetchChannel : public NdnRtcObject
        {
        public:
            
            virtual std::string getLogFile() const
            { return string("fetch.log"); }
            
            virtual ParamsStruct
            getParameters() const { return params_; }
            
            virtual shared_ptr<FrameBuffer>
            getFrameBuffer() const { return frameBuffer_; }
            
            virtual shared_ptr<Pipeliner>
            getPipeliner() const { return pipeliner_; }
            
            virtual shared_ptr<InterestQueue>
            getInterestQueue() const { return interestQueue_; }
            
            virtual shared_ptr<IPacketAssembler>
            getPacketAssembler() const { return packetAssembler_; }
            
            virtual shared_ptr<PacketPlayback>
            getPacketPlayback() const { return packetPlayback_; }
            
            virtual shared_ptr<RttEstimation>
            getRttEstimation() const { return rttEstimation_; }
            
            virtual shared_ptr<ChaseEstimation>
            getChaseEstimation() const { return chaseEstimation_; }
            
            virtual shared_ptr<BufferEstimator>
            getBufferEstimator() const { return bufferEstimator_; }
            
        protected:
            shared_ptr<FrameBuffer> frameBuffer_;
            shared_ptr<Pipeliner> pipeliner_;
            shared_ptr<InterestQueue> interestQueue_;
            shared_ptr<IPacketAssembler> packetAssembler_;
            shared_ptr<PacketPlayback> packetPlayback_;
            shared_ptr<RttEstimation> rttEstimation_;
            shared_ptr<ChaseEstimation> chaseEstimation_;
            shared_ptr<BufferEstimator> bufferEstimator_;
        };
    }
}

#endif /* defined(__ndnrtc__fetch_channel__) */
