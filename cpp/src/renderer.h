//
//  canvas-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/19/13
//

#ifndef __ndnrtc__canvas_renderer__
#define __ndnrtc__canvas_renderer__

#include <modules/video_render/video_render_impl.h>

#include "ndnrtc-common.h"
#include "camera-capturer.h"
#include "ndINrtc.h"

#include "jsapi.h"
#include "jsdbgapi.h"

// XPCOM-related
#include "nsThreadUtils.h"
#include "nsIDOMCanvasRenderingContext2D.h"


namespace ndnrtc {
    
    class NdnRendererParams : public NdnParams
    {
    public:
        // public methods go here
        unsigned int getRenderAreaWidth() { return 640; };
        unsigned int getRenderAreaHeight() { return 480; };
    };
    
    class NdnRenderer : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnRenderer(int rendererId, NdnRendererParams *params_);
        ~NdnRenderer();
        
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
    private:
        // private static attributes go here
        // private attributes go here
        int rendererId_;
#warning make static in long run
        webrtc::VideoRender *render_ = NULL;        
        webrtc::VideoRenderCallback *frameSink_;
        
        // private methods go here
        NdnRendererParams *getParams(){ return static_cast<NdnRendererParams*>(params_); };
    };
}

#endif /* defined(__ndnrtc__canvas_renderer__) */
