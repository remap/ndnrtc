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
#include "renderer.h"
#include "video-decoder.h"
#include "statistics.h"

namespace ndnrtc {
    namespace new_api {
        class VideoConsumer : public Consumer
        {
        public:
            VideoConsumer(const ParamsStruct& params,
                          const shared_ptr<InterestQueue>& interestQueue);
            ~VideoConsumer();
            
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
            
        private:
            shared_ptr<NdnRenderer> renderer_;
            shared_ptr<NdnVideoDecoder> decoder_;
        };
    }
}

#endif /* defined(__ndnrtc__video_consumer__) */
