//
//  chase-estimator.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__chase_estimator__
#define __ndnrtc__chase_estimator__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api {
        class ChaseEstimation : public ndnlog::new_api::ILoggingObject
        {
        public:
            
            static const unsigned int SampleSize;
            static const double ChangeThreshold;
            static const double FilterCoeff;
            static const int InclineEstimatorSample;
            static const int MinOccurences;
            
            ChaseEstimation(unsigned int sampleSize = SampleSize,
                            double changeThreshold = ChangeThreshold,
                            double filterCoeff = FilterCoeff);
            ~ChaseEstimation();
            
            void
            trackArrival();
            
            double
            getArrivalEstimation();
            
            bool
            isArrivalStable();
            
            void
            reset();
          
        private:
            unsigned int sampleSize_;
            double changeThreshold_;
            unsigned int arrivalDelayEstimatorId_,
                        inclineEstimator_;
            int64_t lastArrivalTimeMs_, startTime_;

            int nStabilizedOccurences_;
            bool stabilized_;
            double stabilizedValue_, lastCheckedValue_;
        };
    }
}

#endif /* defined(__ndnrtc__chase_estimator__) */
