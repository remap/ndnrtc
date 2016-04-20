//
//  external-capturer.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "external-capturer.hpp"

using namespace webrtc;
using namespace ndnrtc;

ExternalCapturer::ExternalCapturer():
BaseCapturer()
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
    incomingTimestampMs_ = 0;
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
    if (!isCapturing_)
    {
        startCapture();
        isCapturing_ = true;
    }
}

void ExternalCapturer::capturingStopped()
{
    if (isCapturing_)
    {
        stopCapture();
        isCapturing_ = false;
    }
}

int ExternalCapturer::incomingArgbFrame(const unsigned int width,
                                        const unsigned int height,
                                        unsigned char* argbFrameData,
                                        unsigned int frameSize)
{
    if (isCapturing_)
    {
        LogTraceC << "incoming ARGB frame" << std::endl;
        int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
        
        if (incomingTimestampMs_ != 0)
        {
            int64_t delay = timestamp - incomingTimestampMs_;
            
            ((delay > FRAME_DELAY_DEADLINE)? LogWarnC : LogTraceC)
            << "incoming frame delay " << delay << std::endl;
        }
        
        incomingTimestampMs_ = timestamp;
        
        // make conversion to I420
        const VideoType commonVideoType =
        RawVideoTypeToCommonVideoVideoType(kVideoARGB);
        
        int stride_y = width;
        int stride_uv = (width + 1) / 2;
        int target_width = width;
        int target_height = height;
        
        LogTraceC << "creating I420 frame..." << std::endl;
        int ret = convertedFrame_.CreateEmptyFrame(target_width,
                                                   abs(target_height),
                                                   stride_y,
                                                   stride_uv, stride_uv);
        if (ret < 0)
            return notifyError(RESULT_ERR, "failed to allocate I420 frame");
        
        LogTraceC << "converting RGB to I420..." << std::endl;
        const int conversionResult = ConvertToI420(commonVideoType,
                                                   argbFrameData,
                                                   0, 0,  // No cropping
                                                   width, height,
                                                   frameSize,
                                                   kVideoRotation_0,
                                                   &convertedFrame_);
        if (conversionResult < 0)
            return notifyError(RESULT_ERR, "Failed to convert capture frame to I420");
        
        LogTraceC << "delivering frame..." << std::endl;
        deliverCapturedFrame(convertedFrame_);
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

int ExternalCapturer::incomingI420Frame(const unsigned int width,
                                        const unsigned int height,
                                        const unsigned int strideY,
                                        const unsigned int strideU,
                                        const unsigned int strideV,
                                        const unsigned char* yBuffer,
                                        const unsigned char* uBuffer,
                                        const unsigned char* vBuffer)
{
    if (isCapturing_)
    {
        LogTraceC << "incoming YUV frame" << std::endl;
        int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
        
        if (incomingTimestampMs_ != 0)
        {
            int64_t delay = timestamp - incomingTimestampMs_;
            
            ((delay > FRAME_DELAY_DEADLINE)? LogWarnC : LogTraceC)
            << "incoming frame delay " << delay << std::endl;
        }
        
        incomingTimestampMs_ = timestamp;
        
        convertedFrame_.CreateFrame(yBuffer, uBuffer, vBuffer,
                                    width, height,
                                    strideY, strideU, strideV);
        
        LogTraceC << "delivering frame..." << std::endl;
        deliverCapturedFrame(convertedFrame_);
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}
