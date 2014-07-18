//
//  rate-control.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__rate_control__
#define __ndnrtc__rate_control__

#include "ndnrtc-common.h"
#include "interest-queue.h"
#include "rate-adaptation-module.h"

namespace ndnrtc {
    namespace new_api {
        class Consumer;
        
        class RateControl : public ndnlog::new_api::ILoggingObject {
        public:
            
            RateControl(const shared_ptr<Consumer>& consumer);
            ~RateControl(){}
            
            int
            initialize(const ParamsStruct& params);
            
            void
            start();
            
            void
            stop();
            
            static void
            streamArrayForArcModule(const ParamsStruct& params,
                                       StreamEntry** streamArray);
            
            void
            interestExpressed(const shared_ptr<const Interest>& interest);
            
            void
            interestTimeout(const shared_ptr<const Interest>& interest);
            
            void
            dataReceived(const ndn::Data& data, unsigned int nRtx);
            
        private:
            unsigned int streamId_ = 0;
            shared_ptr<Consumer> consumer_;
            shared_ptr<IRateAdaptationModule> arcModule_;
            
            webrtc::ThreadWrapper& arcWatchingThread_;
            webrtc::EventWrapper &arcWatchTimer_;
            
            static bool
            arcWatchingRoutine(void *obj)
            { return ((RateControl*)obj)->checkArcStatus(); }
            
            bool
            checkArcStatus();
        };
    }
}

#endif /* defined(__ndnrtc__rate_control__) */
