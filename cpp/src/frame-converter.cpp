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

WebRtcVideoFrame& RawFrameConverter::operator<<(const ArgbRawFrameWrapper& wr)
{             
	// make conversion to I420
#warning this needs to be tested with capturing from video devices
	// const VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(kVideoARGB);
	const VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(kVideoBGRA);

	int stride_y = wr.width_;
	int stride_uv = (wr.width_ + 1) / 2;
	int target_width = wr.width_;
	int target_height = wr.height_;

	int ret = convertedFrame_.CreateEmptyFrame(target_width,
		abs(target_height),
		stride_y,
		stride_uv, stride_uv);
	
	if (ret < 0)
		throw std::runtime_error("Failed to allocate I420 frame");

	const int conversionResult = ConvertToI420(commonVideoType,
											   wr.argbFrameData_,
                                               0, 0,  // No cropping
                                               wr.width_, wr.height_,
                                               wr.frameSize_,
                                               kVideoRotation_0,
                                               &convertedFrame_);
	if (conversionResult < 0)
		throw std::runtime_error("Failed to convert capture frame to I420");

	return convertedFrame_;
}

WebRtcVideoFrame& RawFrameConverter::operator<<(const I420RawFrameWrapper& wr)
{
	convertedFrame_.CreateFrame(wr.yBuffer_, wr.uBuffer_, wr.vBuffer_,
		wr.width_, wr.height_,
		wr.strideY_, wr.strideU_, wr.strideV_);

	return convertedFrame_;
}
