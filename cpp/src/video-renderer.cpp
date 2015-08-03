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
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
//******************************************************************************
ExternalVideoRendererAdaptor::ExternalVideoRendererAdaptor(IExternalRenderer* externalRenderer): externalRenderer_(externalRenderer)
{
    description_ = "external-renderer";
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
ExternalVideoRendererAdaptor::onDeliverFrame(WebRtcVideoFrame &frame,
                                             double timestamp)
{
    uint8_t *rgbFrameBuffer = externalRenderer_->getFrameBuffer(frame.width(),
                                                                frame.height());
    if (rgbFrameBuffer)
    {
        ConvertFromI420(frame, kBGRA, 0, rgbFrameBuffer);
        externalRenderer_->renderBGRAFrame(NdnRtcUtils::millisecondTimestamp(),
                                          frame.width(), frame.height(),
                                          rgbFrameBuffer);
    }
}