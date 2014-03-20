//
//  rtt-estimator.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//
#ifndef __ndnrtc__rtt_estimator__
#define __ndnrtc__rtt_estimator__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api {
        
        class RttEstimation {
        public:
            static const double RttStartEstimate;
            
            static RttEstimation&
            sharedInstance();
            
            /**
             * Updates current RTT estimation
             * @param expressTime Timestamp (milliseconds) of segment interest 
             * expression by the consumer
             * @param consumeTime Timestamp (milliseconds) when the data has 
             * been received by the consumer
             * @param generationDelay Generation delay for the data, provided by
             * the producer
             * @return Updated RTT estimation value
             */
            double updateEstimation(int64_t expressTime,
                                    int64_t consumeTime, int64_t generationDelay);
            
            double getCurrentEstimation() const;
            
        private:
            
            RttEstimation();
            ~RttEstimation();
            
            static RttEstimation sharedRttEstimation_;
            unsigned int estimatorId_;
        };
    }
}

#endif /* defined(__ndnrtc__rtt_estimator__) */
