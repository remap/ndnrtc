// 
// frame-converter.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include "frame-converter.hpp"
#include <stdexcept>

using namespace ndnrtc;
using namespace webrtc;
#include <iostream>
WebRtcVideoFrame RawFrameConverter::operator<<(const struct _8bitFixedSizeRawFrameWrapper& wr)
{
    // NOTE: after many hours debugging and reading bytes, it is still uknown 
    // to me, why, vpx encoder is given ARGB buffer and videoType == kARGB, the resulting
    // encoding is messed up, as if it reads data incorrectly (endianness?)
    // anyhow, I figured, that if for ARGB input set videoType kBGRA and vice versa,
    // encoding is correct and different ndnrtc clients decode image correctly
    // thus, i comment intuitively correct line below and instead, use flip data 
    // representations for the opposite of the input data type
    // return convert(wr, RawVideoTypeToCommonVideoVideoType(wr.isArgb_ ? kVideoARGB : kVideoBGRA));
    return convert(wr, RawVideoTypeToCommonVideoVideoType(wr.isArgb_ ? kVideoBGRA : kVideoARGB));
}

WebRtcVideoFrame RawFrameConverter::convert(const ArgbRawFrameWrapper& wr, const VideoType& commonVideoType)
{             
	// make conversion to I420

	int stride_y = wr.width_;
	int stride_uv = (wr.width_ + 1) / 2;
	int target_width = wr.width_;
	int target_height = wr.height_;

	frameBuffer_ = I420Buffer::Create(wr.width_, wr.height_, stride_y, stride_uv, stride_uv);

	if (!frameBuffer_)
		throw std::runtime_error("Failed to allocate I420 frame");

	const int conversionResult = ConvertToI420(commonVideoType,
											   wr.frameData_,
                                               0, 0,  // No cropping
                                               wr.width_, wr.height_,
                                               wr.frameSize_,
                                               kVideoRotation_0,
                                               frameBuffer_.get());
	if (conversionResult < 0)
		throw std::runtime_error("Failed to convert capture frame to I420");

	return WebRtcVideoFrame(frameBuffer_, webrtc::kVideoRotation_0, 0);
}

WebRtcVideoFrame RawFrameConverter::operator<<(const I420RawFrameWrapper& wr)
{
	frameBuffer_ = I420Buffer::Create(wr.width_, wr.height_, wr.strideY_, wr.strideU_, wr.strideV_);

	unsigned int ySize = wr.strideY_*wr.height_;
	unsigned int uSize = wr.strideU_*(wr.height_+1)/2;
	unsigned int vSize = wr.strideV_*(wr.height_+1)/2;
	memcpy(frameBuffer_->MutableDataY(), wr.yBuffer_, ySize);
	memcpy(frameBuffer_->MutableDataU(), wr.uBuffer_, uSize);
	memcpy(frameBuffer_->MutableDataV(), wr.vBuffer_, vSize);

	return WebRtcVideoFrame(frameBuffer_, webrtc::kVideoRotation_0, 0);
}

WebRtcVideoFrame RawFrameConverter::operator<<(const YUV_NV21FrameWrapper& wr)
{             
	// make conversion to I420
	const VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(kVideoNV21);

	int stride_y = wr.width_;
	int stride_uv = (wr.width_ + 1) / 2;
	int target_width = wr.width_;
	int target_height = wr.height_;

	frameBuffer_ = I420Buffer::Create(wr.width_, wr.height_, stride_y, stride_uv, stride_uv);

	if (!frameBuffer_)
		throw std::runtime_error("Failed to allocate I420 frame");

	const int conversionResult = ConvertToI420(commonVideoType,
											   wr.yBuffer_,
                                               0, 0,  // No cropping
                                               wr.width_, wr.height_,
                                               wr.strideY_+wr.strideUV_,
                                               kVideoRotation_0,
                                               frameBuffer_.get());
	if (conversionResult < 0)
		throw std::runtime_error("Failed to convert capture frame to I420");

	return WebRtcVideoFrame(frameBuffer_, webrtc::kVideoRotation_0, 0);
}
