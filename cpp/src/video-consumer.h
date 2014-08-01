//
//  video-consumer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_consumer__
#define __ndnrtc__video_consumer__

#include "consumer.h"
#include "video-renderer.h"
#include "video-decoder.h"
#include "statistics.h"

namespace ndnrtc {
    namespace new_api {
        class VideoConsumer : public Consumer,
                              public IInterestQueueCallback
        {
        public:
            VideoConsumer(const ParamsStruct& params,
                          const boost::shared_ptr<InterestQueue>& interestQueue,
                          const boost::shared_ptr<RttEstimation>& rttEstimation = boost::shared_ptr<RttEstimation>(),
                          IExternalRenderer* const externalRenderer = 0);
            virtual ~VideoConsumer();
            
            int
            init();
            
            int
            start();
            
            int
            stop();
            
            void
            reset();
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
            void
            onInterestIssued(const boost::shared_ptr<const ndn::Interest>& interest);
            
            void
            onStateChanged(const int& oldState, const int& newState);
            
        private:
            boost::shared_ptr<NdnVideoDecoder> decoder_;
            
            boost::shared_ptr<IVideoRenderer>
            getRenderer()
            { return boost::dynamic_pointer_cast<IVideoRenderer>(renderer_); }
            
            
            void
            onTimeout(const boost::shared_ptr<const Interest>& interest);
        };
    }
}

#endif /* defined(__ndnrtc__video_consumer__) */
