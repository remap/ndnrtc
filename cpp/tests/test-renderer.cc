// 
// test-renderer.cc
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>

#include "tests-helpers.h"
#include "gtest/gtest.h"
#include "client/src/renderer.h"

using namespace std;

TEST(TestRenderer, TestCreate)
{
	RendererInternal r("client1-camera");
}

TEST(TestRenderer, Test1Frame)
{
	RendererInternal r("/tmp/client1-camera");
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	r.renderBGRAFrame(1457733705984, width, height, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera-0-640x480.argb").c_str()).good());
	remove("/tmp/client1-camera-640x480-0.argb");
}

TEST(TestRenderer, Test2Frames)
{
	RendererInternal r("/tmp/client1-camera");
	
	uint8_t *buffer = r.getFrameBuffer(640, 480);
	memset(buffer, 0, 640*480*4);
	r.renderBGRAFrame(1457733705984, 640, 480, buffer);

	buffer = r.getFrameBuffer(1280, 720);
	memset(buffer, 0, 1280*720*4);
	r.renderBGRAFrame(1457733705994, 1280, 720, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera-0-640x480.argb").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera-1-1280x720.argb").c_str()).good());
	remove("/tmp/client1-camera-0-640x480.argb");
	remove("/tmp/client1-camera-1-1280x720.argb");
}

TEST(TestRenderer, TestThrowOnSinkCreation)
{
	RendererInternal r("/client1-camera");
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestThrowOnSinkCreation2)
{
	RendererInternal r("");
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression)
{
	RendererInternal r("/client1-camera", true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression2)
{
	RendererInternal r("", true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestRenderFrameNoSink)
{
	RendererInternal r("", true);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	r.renderBGRAFrame(1457733705984, width, height, buffer);

	ASSERT_FALSE(std::ifstream(string("/tmp/client1-camera-0-640x480.argb").c_str()).good());
}

TEST(TestRenderer, TestWrongArgs)
{
	RendererInternal r("/tmp/client1-camera");
	
	uint8_t *buf = r.getFrameBuffer(640, 480);
	EXPECT_ANY_THROW(r.renderBGRAFrame(1457733705984, 1280, 720, buf));
}

TEST(TestRenderer, TestWrongCallSequence)
{
	RendererInternal r("/tmp/client1-camera");
	
	EXPECT_ANY_THROW(r.renderBGRAFrame(1457733705984, 1280, 720, nullptr));
}

TEST(TestRenderer, TestFrameCorrectness)
{
	RendererInternal r("/tmp/client1-camera");
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	for (int i = 0; i < width*height*4; ++i)
		buffer[i] = (i%256);

	r.renderBGRAFrame(1457733705984, width, height, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera-0-640x480.argb").c_str()).good());

	FILE *f = fopen("/tmp/client1-camera-0-640x480.argb", "rb");
	fseek (f , 0 , SEEK_END);
  	long lSize = ftell(f);
  	rewind (f);

  	ASSERT_EQ(lSize, width*height*4);
  	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*lSize);

  	fread(buf, sizeof(uint8_t), lSize, f);

	for (int i = 0; i < width*height*4; ++i)
		ASSERT_EQ(buf[i], (i%256));

	free(buf);
	remove("/tmp/client1-camera-640x480-0.argb");
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
