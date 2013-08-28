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

namespace ndnrtc {
    
    class NdnRendererParams : public NdnParams
    {
    public:
        // static public
        static NdnRendererParams* defaultParams()
        {
            NdnRendererParams *p = new NdnRendererParams();
            
            p->setIntParam(ParamNameWindowWidth, 640);
            p->setIntParam(ParamNameWindowHeight, 480);
            
            return p;
        }
        
        // parameters names
        static const std::string ParamNameWindowWidth;
        static const std::string ParamNameWindowHeight;
        
        // public methods
        int getWindowWidth(int *width) { return getParamAsInt(ParamNameWindowWidth, width); };
        int getWindowHeight(int *height) { return getParamAsInt(ParamNameWindowHeight, height); };
    };
    
    class NdnRenderer : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnRenderer(int rendererId, NdnRendererParams *params_);
        ~NdnRenderer();
        
        int startRendering();
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
    private:
        // private static attributes go here
        // private attributes go here
        int rendererId_;
        bool initialized_ = false;
#warning make static in long run
        webrtc::VideoRender *render_ = NULL;        
        webrtc::VideoRenderCallback *frameSink_ = NULL;
        
        // private methods go here
        NdnRendererParams *getParams(){ return static_cast<NdnRendererParams*>(params_); };
    };
}

#endif /* defined(__ndnrtc__canvas_renderer__) */
