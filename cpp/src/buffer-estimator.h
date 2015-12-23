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

#include "simple-log.h"

namespace ndnrtc {
    namespace new_api
    {
        class RttEstimation;
        
        class BufferEstimator : public ndnlog::new_api::ILoggingObject
        {
        public:
            
            static const int64_t MinBufferSizeMs;
            
            BufferEstimator(double alpha, double beta):alpha_(alpha), beta_(beta), minBufferSizeMs_(MinBufferSizeMs){}
            BufferEstimator(double alpha, double beta,
                            const boost::shared_ptr<RttEstimation>& rttEstimation,
                            int64_t minBufferSizeMs = MinBufferSizeMs);
            ~BufferEstimator(){}
            
            void
            setProducerRate(double producerRate) DEPRECATED;
            
            void
            setRttEstimation(const boost::shared_ptr<RttEstimation>& rttEstimation)
            { rttEstimation_ = rttEstimation; }
            
            void
            setMinimalBufferSize(int64_t minimalBufferSize)
            { minBufferSizeMs_ = minimalBufferSize; }
            
            int64_t
            getTargetSize();
            
        private:
            double alpha_, beta_;
            boost::shared_ptr<RttEstimation> rttEstimation_;
            int64_t minBufferSizeMs_;
        };
    }
}

#endif /* defined(__ndnrtc__buffer_estimator__) */
