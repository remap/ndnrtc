//
//  audio-consumer.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__audio_consumer__
#define __ndnrtc__audio_consumer__

#include "consumer.h"
#include "statistics.h"
#include "audio-renderer.h"

namespace ndnrtc {
    namespace new_api {
        class AudioConsumer : public Consumer {
        public:
            AudioConsumer(const ParamsStruct& params,
                          const boost::shared_ptr<InterestQueue>& interestQueue,
                          const boost::shared_ptr<RttEstimation>& rttEstimation = boost::shared_ptr<RttEstimation>());
            ~AudioConsumer();
            
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
            boost::shared_ptr<AudioRenderer>
            getRenderer()
            { return boost::dynamic_pointer_cast<AudioRenderer>(renderer_); }
            
        };
    }
}

#endif /* defined(__ndnrtc__audio_consumer__) */
