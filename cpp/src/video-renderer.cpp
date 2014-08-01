//
//  video-renderer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#undef DEBUG

#include "video-renderer.h"
#import "objc/cocoa-renderer.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
VideoRenderer::VideoRenderer(int rendererId, const ParamsStruct &params) :
rendererId_(rendererId)
{
    params_ = params;
    description_ = "vrenderer";
}

VideoRenderer::~VideoRenderer()
{
    stopRendering();
    
    if (renderWindow_)
    {
        destroyCocoaRenderWindow(renderWindow_);
        renderWindow_ = NULL;
    }
    
    if (render_)
    {
        VideoRender::DestroyVideoRender(render_);
        render_ = NULL;
    }
}
//******************************************************************************
#pragma mark - public
int VideoRenderer::init()
{
    int res = RESULT_OK;
    
    unsigned int width = ParamsStruct::validateLE(params_.renderWidth, MaxWidth,
                                                  res, DefaultParams.renderWidth);
    unsigned int height = ParamsStruct::validateLE(params_.renderHeight, MaxHeight,
                                                   res, DefaultParams.renderHeight);
    
    renderWindow_ = createCocoaRenderWindow("", width, height);
    render_ = VideoRender::CreateVideoRender(rendererId_, renderWindow_, false, kRenderCocoa);
    
    if (render_ == nullptr)
        return notifyError(-1, "can't initialize renderer");
    
    frameSink_ = render_->AddIncomingRenderStream(rendererId_, 0, 0.f, 0.f, 1.f, 1.f);
    
    if (res)
        notifyError(RESULT_WARN, "renderer params were not verified. \
                    using these instead: width %d, height %d", width, height);
    
    return res;
}
int VideoRenderer::startRendering(const string &windowName)
{
    setWindowTitle(windowName.c_str(), renderWindow_);
    
    if (render_->StartRender(rendererId_) < 0)
    {
        return notifyError(RESULT_ERR, "can't start rendering");
    }
    
    isRendering_ = true;
    
    return 0;
}
int VideoRenderer::stopRendering()
{
    if (render_)
    {
        render_->StopRender(rendererId_);
    }
    
    isRendering_ = false;
    return RESULT_OK;
}
//******************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void VideoRenderer::onDeliverFrame(I420VideoFrame &frame, double timestamp)
{
    if (isRendering_)
    {
        LogTraceC
        << "render frame at " << NdnRtcUtils::millisecondTimestamp() << endl;
        
        int res = frameSink_->RenderFrame(rendererId_, frame);
        
        if (res < 0)
            notifyError(RESULT_ERR, "render error %d", res);
        else
            LogTraceC << "render result " << res << endl;
    }
    else
        notifyError(RESULT_ERR, "render not started");
}

//******************************************************************************
//******************************************************************************
ExternalVideoRendererAdaptor::ExternalVideoRendererAdaptor(IExternalRenderer* externalRenderer): externalRenderer_(externalRenderer)
{
    
}

int
ExternalVideoRendererAdaptor::init()
{
    return RESULT_OK;
}

int
ExternalVideoRendererAdaptor::startRendering(const std::string &windowName)
{
    isRendering_ = true;
    return RESULT_OK;
}

int
ExternalVideoRendererAdaptor::stopRendering()
{
    isRendering_ = false;
    return RESULT_OK;
}

//******************************************************************************
void
ExternalVideoRendererAdaptor::onDeliverFrame(webrtc::I420VideoFrame &frame,
                                             double timestamp)
{
    uint8_t *rgbFrameBuffer = externalRenderer_->getFrameBuffer(frame.width(),
                                                                frame.height());
    if (rgbFrameBuffer)
    {
        ConvertFromI420(frame, kRGB24, 0, rgbFrameBuffer);
        externalRenderer_->renderRGBFrame(NdnRtcUtils::millisecondTimestamp(),
                                          frame.width(), frame.height(),
                                          rgbFrameBuffer);
    }
}