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

#include "ndnrtc-common.h"
#include "params.h"
#include "interfaces.h"

namespace ndnrtc {
    
    namespace new_api {
        class INdnRtcComponentCallback {
        public:
            virtual ~INdnRtcComponentCallback(){}
            virtual void onError(const char *errorMessage) = 0;
        };
        
        class NdnRtcComponent : public ndnlog::new_api::ILoggingObject,
                                public INdnRtcComponentCallback
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
            
            virtual void onError(const char *errorMessage);
            
            // ILoggingObject interface conformance
            virtual std::string
            getDescription() const;
            
            virtual bool
            isLoggingEnabled() const
            { return true; }
            
        protected:
            // critical section for observer's callbacks
            webrtc::CriticalSectionWrapper &callbackSync_;
            INdnRtcComponentCallback *callback_ = nullptr;
            
            // protected methods go here
            int notifyError(const int ecode, const char *format, ...);
            bool hasCallback() { return callback_ != NULL; }
        };
    }
    
    class NdnRtcObject :    public ndnlog::new_api::ILoggingObject,
                            public INdnRtcObjectObserver
    {
    public:
        // construction/desctruction
        NdnRtcObject(const ParamsStruct &params = DefaultParams);
        NdnRtcObject(const ParamsStruct &params,
                     INdnRtcObjectObserver *observer);
        virtual ~NdnRtcObject();
        
        // public methods go here
        void setObserver(INdnRtcObjectObserver *observer) { observer_ = observer; }
        
        virtual void onErrorOccurred(const char *errorMessage);
        
        // ILoggingObject interface conformance
        virtual std::string
        getDescription() const;
        
        virtual bool
        isLoggingEnabled() const
        {
            return true;
        }
        
    protected:
        // critical section for observer's callbacks
        webrtc::CriticalSectionWrapper &callbackSync_;
        INdnRtcObjectObserver *observer_ = nullptr;
        ParamsStruct params_;
        
        // protected methods go here
        int notifyError(const int ecode, const char *format, ...);
        int notifyErrorNoParams();
        int notifyErrorBadArg(const std::string &paramName);
        bool hasObserver() { return observer_ != nullptr; };
    };
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
