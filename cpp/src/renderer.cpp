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
NdnRenderer::NdnRenderer(int rendererId, const ParamsStruct &params) :
NdnRtcObject(params),
rendererId_(rendererId)
{
}
NdnRenderer::~NdnRenderer()
{
    if (render_)
        render_->StopRender(rendererId_);
    
    VideoRender::DestroyVideoRender(render_);
}
//********************************************************************************
#pragma mark - public
int NdnRenderer::init()
{
    int res = RESULT_OK;
    
    unsigned int width = ParamsStruct::validateLE(params_.renderWidth, MaxWidth,
                                               res, DefaultParams.renderWidth);
    unsigned int height = ParamsStruct::validateLE(params_.renderHeight, MaxHeight,
                                                res, DefaultParams.renderHeight);
    
    render_ = VideoRender::CreateVideoRender(rendererId_, createCocoaRenderWindow(rendererId_, width, height), false, kRenderCocoa);
    
    if (render_ == nullptr)
        return notifyError(-1, "can't initialize renderer");
    
    frameSink_ = render_->AddIncomingRenderStream(rendererId_, 0, 0.f, 0.f, 1.f, 1.f);
    
    if (res)
        notifyError(RESULT_WARN, "renderer params were not verified. \
                    using these instead: width %d, height %d", width, height);
    
    return res;
}
int NdnRenderer::startRendering()
{
    if (render_->StartRender(rendererId_) < 0)
        return notifyError(RESULT_ERR, "can't start rendering");
    
    initialized_ = true;
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnRenderer::onDeliverFrame(I420VideoFrame &frame)
{
    if (initialized_)
        frameSink_->RenderFrame(rendererId_, frame);
    else
        notifyError(RESULT_ERR, "render not initiazlized");
}

//********************************************************************************
#pragma mark - private
