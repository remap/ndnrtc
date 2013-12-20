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

namespace ndnrtc
{
    /**
     * This class is used for rendering incoming video frames in default 
     * rendering (cocoa) window. Incoming frames are of type I420VideoFrame 
     * (WebRTC-specific) and represent graphical data int YUV format.
     */
    class NdnRenderer : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        NdnRenderer(int rendererId, const ParamsStruct &params);
        ~NdnRenderer();
        
        int init();
        int startRendering(const std::string &windowName = "Renderer");
        int stopRendering();
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
    private:
        int rendererId_;
        bool isRendering_ = false;
        void *renderWindow_ = NULL;

        webrtc::VideoRender *render_ = nullptr;        
        webrtc::VideoRenderCallback *frameSink_ = nullptr;
    };
}

#endif /* defined(__ndnrtc__canvas_renderer__) */
