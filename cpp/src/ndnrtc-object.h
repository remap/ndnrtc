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
#include "ndnrtc-observer.h"

namespace ndnrtc {    
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
