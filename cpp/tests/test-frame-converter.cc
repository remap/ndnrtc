// 
// test-frame-converter.cc
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "frame-converter.hpp"

using namespace ndnrtc;

TEST(TestFrameConverter, TestArgbFrame)
{
	unsigned int w = 640, h = 480, size = w*h*4;
	uint8_t* data = (uint8_t*)malloc(size);

	RawFrameConverter conv;
	WebRtcVideoFrame frame = conv << ArgbRawFrameWrapper({w,h,data,size});

	EXPECT_EQ(w, frame.width());
	EXPECT_EQ(h, frame.height());

	free(data);
}

TEST(TestFrameConverter, TestI420Frame)
{
	unsigned int w = 15, h = 15, strideY = 15, strideUV = 10;
	const int kSizeY = 225;
	const int kSizeUv = 80;
	uint8_t *ybuf = (uint8_t*)malloc(kSizeY);
	uint8_t *ubuf = (uint8_t*)malloc(kSizeUv);
	uint8_t *vbuf = (uint8_t*)malloc(kSizeUv);

	RawFrameConverter conv;
	WebRtcVideoFrame frame = conv << I420RawFrameWrapper({w,h,strideY, strideUV, strideUV, ybuf, ubuf, vbuf});
	
	EXPECT_EQ(w, frame.width());
	EXPECT_EQ(h, frame.height());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
