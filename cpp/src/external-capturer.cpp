//
//  external-capturer.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "external-capturer.hpp"

using namespace webrtc;
using namespace ndnrtc;

ExternalCapturer::ExternalCapturer(const ParamsStruct& params):
BaseCapturer(params)
{
    
}

ExternalCapturer::~ExternalCapturer()
{
}

int ExternalCapturer::init()
{
    BaseCapturer::init();
    return RESULT_OK;
}

int ExternalCapturer::startCapture()
{
    BaseCapturer::startCapture();
    
    return RESULT_OK;
}

int ExternalCapturer::stopCapture()
{
    BaseCapturer::stopCapture();
    
    return RESULT_OK;
}

void ExternalCapturer::capturingStarted()
{
    isCapturing_ = true;    
}

void ExternalCapturer::capturingStopped()
{
    BaseCapturer::stopCapture();
    isCapturing_ = false;
}

int ExternalCapturer::incomingArgbFrame(const unsigned int width,
                                        const unsigned int height,
                                        unsigned char* argbFrameData,
                                        unsigned int frameSize)
{
    // make conversion to I420
    const VideoType commonVideoType =
    RawVideoTypeToCommonVideoVideoType(kVideoARGB);
    
    int stride_y = width;
    int stride_uv = (width + 1) / 2;
    int target_width = width;
    int target_height = height;
    
    int ret = convertedFrame_.CreateEmptyFrame(target_width,
                                             abs(target_height),
                                             stride_y,
                                             stride_uv, stride_uv);
    if (ret < 0)
        return notifyError(RESULT_ERR, "failed to allocate I420 frame");
    
    const int conversionResult = ConvertToI420(commonVideoType,
                                               argbFrameData,
                                               0, 0,  // No cropping
                                               width, height,
                                               frameSize,
                                               kRotateNone,
                                               &convertedFrame_);
    if (conversionResult < 0)
        return notifyError(RESULT_ERR, "Failed to convert capture frame to I420");
    
    deliverCapturedFrame(convertedFrame_);
    
    return RESULT_OK;
}
