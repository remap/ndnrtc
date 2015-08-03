//
//  ndnrtc-object.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__ndnrtc_object__
#define __ndnrtc__ndnrtc_object__

#include <boost/thread.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "ndnrtc-common.h"
#include "params.h"
#include "interfaces.h"

namespace ndnrtc {
    namespace new_api {
        class INdnRtcComponentCallback {
        public:
            virtual ~INdnRtcComponentCallback(){}
            virtual void onError(const char *errorMessage,
                                 const int errorCode = -1) = 0;
        };
        
        class NdnRtcComponent :public ndnlog::new_api::ILoggingObject,
        public INdnRtcComponentCallback,
        public boost::enable_shared_from_this<NdnRtcComponent>
        {
        public:
            // construction/desctruction
            NdnRtcComponent();
            NdnRtcComponent(INdnRtcComponentCallback *callback);
            virtual ~NdnRtcComponent();
            
            virtual void registerCallback(INdnRtcComponentCallback *callback)
            { callback_ = callback; }
            virtual void deregisterCallback()
            { callback_ = NULL; }
            
            virtual void onError(const char *errorMessage, const int errorCode);
            
            // ILoggingObject interface conformance
            virtual std::string
            getDescription() const;
            
            virtual bool
            isLoggingEnabled() const
            { return true; }
            
        protected:
            boost::atomic<bool> isJobScheduled_;
            boost::mutex callbackMutex_;
            boost::recursive_mutex jobMutex_;
            INdnRtcComponentCallback *callback_ = nullptr;
            
            // protected methods go here
            int notifyError(const int ecode, const char *format, ...);
            bool hasCallback() { return callback_ != NULL; }
            
            boost::thread startThread(boost::function<bool()> threadFunc);
            void stopThread(boost::thread &thread);
            
            void scheduleJob(const unsigned int usecInterval,
                             boost::function<bool()> jobCallback);
            void stopJob();
            
        private:
            bool isTimerCancelled_;
            boost::asio::steady_timer watchdogTimer_;
        };
    }
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
