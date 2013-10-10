//
//  canvas-renderer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/19/13
//

#undef DEBUG

#include "renderer.h"
#import "objc/cocoa-renderer.h"

using namespace ndnrtc;
using namespace webrtc;

//********************************************************************************
//********************************************************************************

//********************************************************************************
#pragma mark - construction/destruction
NdnRenderer::NdnRenderer(int rendererId, NdnParams *params) : NdnRtcObject(params), rendererId_(rendererId)
{
    TRACE("construction");
}
NdnRenderer::~NdnRenderer()
{
    TRACE("destruction");

    if (render_)
        render_->StopRender(rendererId_);
    VideoRender::DestroyVideoRender(render_);
}
//********************************************************************************
#pragma mark - public
int NdnRenderer::init()
{
    int width, height;
    
    if (getParams()->getWindowWidth(&width) < 0)
        return notifyErrorBadArg(NdnRendererParams::ParamNameWindowWidth);
    if (getParams()->getWindowHeight(&height) < 0)
        return notifyErrorBadArg(NdnRendererParams::ParamNameWindowHeight);
    
    render_ = VideoRender::CreateVideoRender(rendererId_, createCocoaRenderWindow(rendererId_, width, height), false, kRenderCocoa);
    
    if (render_ == nullptr)
        return notifyError(-1, "can't initialize renderer");
    
    frameSink_ = render_->AddIncomingRenderStream(rendererId_, 0, 0.f, 0.f, 1.f, 1.f);
    
    return 0;
}
int NdnRenderer::startRendering()
{
    if (render_->StartRender(rendererId_) < 0)
        return notifyError(-1, "can't start rendering");
    
    initialized_ = true;
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnRenderer::onDeliverFrame(I420VideoFrame &frame)
{
    TRACE("rendering frame");
    if (initialized_)
        frameSink_->RenderFrame(rendererId_, frame);
    else
        notifyError(-1, "render not initiazlized");
}

//********************************************************************************
#pragma mark - private
