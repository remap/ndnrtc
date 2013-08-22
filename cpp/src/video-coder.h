//
//  video-coder.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__video_coder__
#define __ndnrtc__video_coder__

#include "ndnrtc-common.h"
#include "camera-capturer.h"

namespace ndnrtc {
    class IEncodedFrameConsumer;
    
    class NdnVideoCoderParams : public NdnParams {
    public:
        // construction/desctruction
        NdnVideoCoderParams(){};
        ~NdnVideoCoderParams(){};
    }
    
    class NdnVideoCoder : public NdnRtcObject, public IRawFrameConsumer {
    public:
        // construction/desctruction
        NdnVideoCoder(NdnVideoCoderParams *params);
        ~NdnVideoCoder() {};
        
        // public methods go here
        
    private:
        // private attributes go here
        IEncodedFrameConsumer *frameConsumer_;
        
        // private methods go here
        NdnVideoCoderParams *getParams() { return static_cast<NdnVideoCoderParams*>(params_); };
    }
    
    class IEncodedFrameConsumer : public INdnRtcObjectObserver {
    public:
        void onEncodedFrameDelivered() = 0;
    }
}

#endif /* defined(__ndnrtc__video_coder__) */
