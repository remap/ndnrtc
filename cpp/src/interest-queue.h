//
//  interest-queue.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__interest_queue__
#define __ndnrtc__interest_queue__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api {
        class InterestQueue {
        public:
            void enqueueInterest(const Interest& interest, int64_t priority);
        };
    }
}

#endif /* defined(__ndnrtc__interest_queue__) */
