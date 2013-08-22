//
//  ndnrtc-object.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__ndnrtc_object__
#define __ndnrtc__ndnrtc_object__

namespace ndnrtc {
    class INdnRtcObjectObserver {
    public:
        // public methods go here
        void onErrorOccurred(const char *errorMessage) = 0;
    }
    
    class NdnParams {
    public:
        // construction/desctruction
        NdnParams(NdnParams *params) {};
        ~NdnParams() {};
        
        // public methods go here
//        const char* getParamAsString(const char *paramName);
    }
    
    class NdnRtcObject : public INdnRtcObjectObserver {
    public:
        // construction/desctruction
        NdnRtcObject();
        NdnRtcObject(NdnParams *params);
        NdnRtcObject(NdnParams *params, INdnRtcObjectObserver *observer);
        ~NdnRtcObject();
        
        // public methods go here
        void setObserver(INdnRtcObjectObserver *observer) { observer_ = observer; }
        
    protected:
        // protected attributes go here
        INdnRtcObjectObserver *observer_;
        NdnParams *params_;
        
        // protected methods go here
        int notifyError(const int ecode, const char *emsg);
        int notifyNoParams();
        bool hasObserver() { return observer_ != NULL; };
        bool hasParams() { return params_ != NULL; };
    }
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
