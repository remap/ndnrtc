//
//  buffer-estimator.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__buffer_estimator__
#define __ndnrtc__buffer_estimator__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api
    {
        class BufferEstimator {
        public:
            
            static const int64_t MinBufferSizeMs;
            
            BufferEstimator():minBufferSizeMs_(MinBufferSizeMs){}
            ~BufferEstimator(){}
            
            void
            setProducerRate(double producerRate);
            
            void
            setMinimalBufferSize(int64_t minimalBufferSize)
            { minBufferSizeMs_ = minimalBufferSize; }
            
            int64_t
            getTargetSize();
            
        private:
            int64_t minBufferSizeMs_;
        };
    }
}

#endif /* defined(__ndnrtc__buffer_estimator__) */
