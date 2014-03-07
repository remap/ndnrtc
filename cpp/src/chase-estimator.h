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
        class ChaseEstimation {
        public:
            void
            newArrivalDelay(double arrivalDelayMs);
            
            double
            getArrivalEstimation();
            
            bool
            isArrivalStable();
            
        };
    }
}

#endif /* defined(__ndnrtc__chase_estimator__) */
