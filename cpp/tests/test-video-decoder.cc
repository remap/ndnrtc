// 
// test-video-decoder.cc
//
//  Created by Peter Gusev on 09 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/video-decoder.hpp"
#include "tests-helpers.hpp"
#include "mock-objects/encoder-delegate-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;

// #define ENABLE_LOGGING

TEST(TestDecoder, TestCreate)
{
	EXPECT_NO_THROW(
		VideoDecoder decoder(sampleVideoCoderParams(), [](const FrameInfo& fi, const WebRtcVideoFrame &){})
	);
}

TEST(TestDecoder, TestEncodeDecode2K)
{
	int nFrames = 30*5;
	int width = 1280;
	int height = 720;
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

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
	VideoDecoder vdc(vcp, [&nDecoded, width, height](const FrameInfo& fi, const WebRtcVideoFrame &f){
		nDecoded++;
		EXPECT_EQ(f.width(), width);
		EXPECT_EQ(f.height(), height);
	});

    FrameInfo phony = { 0, 0, "/phony/name" };
	EXPECT_CALL(coderDelegate, onEncodedFrame(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&vdc, &nEncoded, &phony](const webrtc::EncodedImage& img){
			nEncoded++;
			vdc.processFrame(phony, img);
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
