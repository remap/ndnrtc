//
//  video-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_renderer__
#define __ndnrtc__video_renderer__

#include <modules/video_render/video_render_impl.h>

#include "renderer.h"
#include "ndnrtc-common.h"
#include "camera-capturer.h"

namespace ndnrtc
{
    /**
     * This class is used for rendering incoming video frames in default
     * rendering (cocoa) window. Incoming frames are of type I420VideoFrame
     * (WebRTC-specific) and represent graphical data int YUV format.
     */
    class VideoRenderer : public new_api::IRenderer, public IRawFrameConsumer,
    public NdnRtcObject
    {
    public:
        VideoRenderer(int rendererId, const ParamsStruct &params);
        virtual ~VideoRenderer();
        
        int
        init();
        
        int
        startRendering(const std::string &windowName = "Renderer");
        
        int
        stopRendering();
        
        void
        onDeliverFrame(webrtc::I420VideoFrame &frame, double timestamp);
        
    protected:
        int rendererId_;
        bool isRendering_ = false;
        void *renderWindow_ = NULL;
        
        webrtc::VideoRender *render_ = nullptr;
        webrtc::VideoRenderCallback *frameSink_ = nullptr;
    };
}


#endif /* defined(__ndnrtc__video_renderer__) */
