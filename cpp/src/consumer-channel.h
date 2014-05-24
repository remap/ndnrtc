//
//  consumer-channel.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__consumer_channel__
#define __ndnrtc__consumer_channel__

#include "video-consumer.h"
#include "audio-consumer.h"
#include "statistics.h"
#include "face-wrapper.h"

namespace ndnrtc {
    namespace new_api {
        class ConsumerChannel : public NdnRtcObject
        {
        public:
            ConsumerChannel(const ParamsStruct& params,
                            const ParamsStruct& audioParams,
                            const shared_ptr<FaceProcessor>& faceProcessor = shared_ptr<FaceProcessor>(nullptr));
            virtual ~ConsumerChannel(){ }
            
            int init();
            int startTransmission();
            int stopTransmission();
            
            void getChannelStatistics(ReceiverChannelStatistics &stat);
            
            std::string
            getDescription() const
            { return "cchannel"; }
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
        protected:
            bool isOwnFace_;
            ParamsStruct audioParams_;
            shared_ptr<VideoConsumer> videoConsumer_;
            shared_ptr<AudioConsumer> audioConsumer_;
            shared_ptr<RttEstimation> rttEstimation_;
            shared_ptr<FaceProcessor> faceProcessor_;
            shared_ptr<InterestQueue> interestQueue_;
            
            // ndn-cpp callbacks
            virtual void onInterest(const shared_ptr<const Name>& prefix,
                                    const shared_ptr<const Interest>& interest,
                                    Transport& transport);
            
            virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>&
                                          prefix);
            
        };
    }
}

#endif /* defined(__ndnrtc__consumer_channel__) */
