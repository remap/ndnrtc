//
//  video-coder.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_coder__
#define __ndnrtc__video_coder__

#include "ndnrtc-common.h"
#include "camera-capturer.h"

namespace ndnrtc {
    class IEncodedFrameConsumer;
    
    class NdnVideoCoderParams : public NdnParams
    {
    public:
        // construction/desctruction
        NdnVideoCoderParams(){};
        ~NdnVideoCoderParams(){};
    };
    
    class NdnVideoCoder : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnVideoCoder(NdnVideoCoderParams *params);
        ~NdnVideoCoder() {};
        
        // public methods go here
        
        // interface conformance - ndnrtc::
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
    private:
        // private attributes go here
        IEncodedFrameConsumer *frameConsumer_;
        
        // private methods go here
        NdnVideoCoderParams *getParams() { return static_cast<NdnVideoCoderParams*>(params_); };
    };
    
    class IEncodedFrameConsumer : public INdnRtcObjectObserver
    {
    public:
       virtual void onEncodedFrameDelivered() = 0;
    };
}

#endif /* defined(__ndnrtc__video_coder__) */
