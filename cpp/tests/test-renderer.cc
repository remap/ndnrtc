// 
// test-renderer.cc
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>

#include "tests-helpers.hpp"
#include "gtest/gtest.h"
#include "client/src/renderer.hpp"

using namespace std;

boost::shared_ptr<IFrameSink> createNewSink(const std::string& path)
{
	return boost::make_shared<FileSink>(path);
}

TEST(TestRenderer, TestCreate)
{
	std::string sinkName = "client1-camera";
	RendererInternal r(sinkName, createNewSink);
}

TEST(TestRenderer, Test1Frame)
{
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	r.renderBGRAFrame(1457733705984, 1, width, height, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
	remove("/tmp/client1-camera.640x480");
}

TEST(TestRenderer, Test2Frames)
{
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	
	uint8_t *buffer = r.getFrameBuffer(640, 480);
	memset(buffer, 0, 640*480*4);
	r.renderBGRAFrame(1457733705984, 1, 640, 480, buffer);

	buffer = r.getFrameBuffer(1280, 720);
	memset(buffer, 0, 1280*720*4);
	r.renderBGRAFrame(1457733705994, 2, 1280, 720, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.1280x720").c_str()).good());
	remove("/tmp/client1-camera.640x480");
	remove("/tmp/client1-camera.1280x720");
}

TEST(TestRenderer, TestThrowOnSinkCreation)
{
	std::string sinkName = "/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestThrowOnSinkCreation2)
{
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink);
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression)
{
	std::string sinkName = "/client1-camera";
	RendererInternal r(sinkName, createNewSink, true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression2)
{
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink, true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestRenderFrameNoSink)
{
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink, true);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	r.renderBGRAFrame(1457733705984, 1, width, height, buffer);

	ASSERT_FALSE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
}

TEST(TestRenderer, TestWrongArgs)
{
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	
	uint8_t *buf = r.getFrameBuffer(640, 480);
	EXPECT_ANY_THROW(r.renderBGRAFrame(1457733705984, 1, 1280, 720, buf));
}

TEST(TestRenderer, TestWrongCallSequence)
{
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	
	EXPECT_ANY_THROW(r.renderBGRAFrame(1457733705984, 1, 1280, 720, nullptr));
}

TEST(TestRenderer, TestFrameCorrectness)
{
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	for (int i = 0; i < width*height*4; ++i)
		buffer[i] = (i%256);

	r.renderBGRAFrame(1457733705984, 1, width, height, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());

	FILE *f = fopen("/tmp/client1-camera.640x480", "rb");
	fseek (f , 0 , SEEK_END);
  	long lSize = ftell(f);
  	rewind (f);

  	ASSERT_EQ(lSize, width*height*4);
  	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*lSize);

  	fread(buf, sizeof(uint8_t), lSize, f);

	for (int i = 0; i < width*height*4; ++i)
		ASSERT_EQ(buf[i], (i%256));

	free(buf);
	remove("/tmp/client1-camera.640x480");
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
