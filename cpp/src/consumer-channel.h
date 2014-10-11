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

//#define SERVICE_CHANNEL

#include "video-consumer.h"
#include "audio-consumer.h"
#include "statistics.h"
#include "face-wrapper.h"
#include "service-channel.h"
#if 0
namespace ndnrtc {
    namespace new_api {
        
        class ConsumerChannel : public NdnRtcObject
        {
        public:
            ConsumerChannel(const ParamsStruct& params,
                            const ParamsStruct& audioParams,
                            IExternalRenderer* const videoRenderer = 0,
                            const boost::shared_ptr<FaceProcessor>& faceProcessor = boost::shared_ptr<FaceProcessor>()) DEPRECATED;
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
            boost::shared_ptr<VideoConsumer> videoConsumer_;
            boost::shared_ptr<AudioConsumer> audioConsumer_;
            boost::shared_ptr<RttEstimation> rttEstimation_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
            boost::shared_ptr<InterestQueue> videoInterestQueue_;
            boost::shared_ptr<InterestQueue> audioInterestQueue_;
        };
    }
}
#endif
#endif /* defined(__ndnrtc__consumer_channel__) */
