// 
// test-video-decoder.cc
//
//  Created by Peter Gusev on 09 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/video-decoder.h"
#include "tests-helpers.h"
#include "mock-objects/encoder-delegate-mock.h"

using namespace ::testing;
using namespace ndnrtc;

// #define ENABLE_LOGGING

TEST(TestDecoder, TestCreate)
{
	EXPECT_NO_THROW(
		VideoDecoder decoder(sampleVideoCoderParams(), [](const WebRtcVideoFrame &){})
	);
}

TEST(TestDecoder, TestEncodeDecode2K)
{
	int nFrames = 30*5;
	int width = 1280;
	int height = 720;
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	std::vector<webrtc::I420VideoFrame> frames;

	for (int f = 0; f < nFrames; ++f)
	{
		for (int j = 0; j < height; ++j)
			for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

		webrtc::I420VideoFrame convertedFrame;
		{
		// make conversion to I420
			const webrtc::VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(webrtc::kVideoARGB);
			int stride_y = width;
			int stride_uv = (width + 1) / 2;
			int target_width = width;
			int target_height = height;

			convertedFrame.CreateEmptyFrame(target_width,
				abs(target_height), stride_y, stride_uv, stride_uv);
			ConvertToI420(commonVideoType, frameBuffer, 0, 0,  // No cropping
				width, height, frameSize, webrtc::kVideoRotation_0, &convertedFrame);
		}
		frames.push_back(convertedFrame);
	}

	bool dropEnabled = true;
	int targetBitrate = 2000;
	VideoCoderParams vcp(sampleVideoCoderParams());
	vcp.startBitrate_ = targetBitrate;
	vcp.maxBitrate_ = targetBitrate;
	vcp.encodeWidth_ = width;
	vcp.encodeHeight_ = height;
	vcp.dropFramesOn_ = dropEnabled;
	MockEncoderDelegate coderDelegate;
	coderDelegate.setDefaults();
	VideoCoder vc(vcp, &coderDelegate);

	int nEncoded = 0;
	int nDecoded = 0;
	VideoDecoder vdc(vcp, [&nDecoded, width, height](const WebRtcVideoFrame &f){
		nDecoded++;
		EXPECT_EQ(f.width(), width);
		EXPECT_EQ(f.height(), height);
	});

	EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&vdc, &nEncoded](const webrtc::EncodedImage& img){
			nEncoded++;
			vdc.processFrame(img);
		}));
	EXPECT_CALL(coderDelegate, onEncodingStarted())
		.Times(nFrames);
	EXPECT_CALL(coderDelegate, onDroppedFrame())
		.Times(AtLeast(0));

	for (auto& f:frames) vc.onRawFrame(f);

	EXPECT_EQ(nEncoded, nDecoded);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
