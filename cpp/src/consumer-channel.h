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
#include "service-channel.h"

namespace ndnrtc {
    namespace new_api {
        class ConsumerChannel : public NdnRtcObject,
                            public IServiceChannelCallback
        {
        public:
            ConsumerChannel(const ParamsStruct& params,
                            const ParamsStruct& audioParams,
                            IExternalRenderer* const videoRenderer = 0,
                            const boost::shared_ptr<FaceProcessor>& faceProcessor = boost::shared_ptr<FaceProcessor>());
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
            boost::shared_ptr<InterestQueue> serviceInterestQueue_;
            boost::shared_ptr<InterestQueue> videoInterestQueue_;
            boost::shared_ptr<InterestQueue> audioInterestQueue_;
            boost::shared_ptr<ServiceChannel> serviceChannel_;
            
            // ndn-cpp callbacks
            virtual void onInterest(const boost::shared_ptr<const Name>& prefix,
                                    const boost::shared_ptr<const Interest>& interest,
                                    Transport& transport);
            
            virtual void onRegisterFailed(const boost::shared_ptr<const Name>&
                                          prefix);
            
            // IServiceChannel interface
            void
            onProducerParametersUpdated(const ParamsStruct& newVideoParams,
                                        const ParamsStruct& newAudioParams);
            
            void
            onUpdateFailedWithTimeout();
            
            void
            onUpdateFailedWithError(const char* errMsg);
            
        };
    }
}

#endif /* defined(__ndnrtc__consumer_channel__) */
