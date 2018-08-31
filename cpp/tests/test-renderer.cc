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
    boost::asio::io_service io;
	std::string sinkName = "client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
}

TEST(TestRenderer, Test1Frame)
{
    boost::asio::io_service io;
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
    ndnrtc::FrameInfo phony;
	r.renderBGRAFrame(phony, width, height, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
	remove("/tmp/client1-camera.640x480");
}

TEST(TestRenderer, Test2Frames)
{
    boost::asio::io_service io;
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	
	uint8_t *buffer = r.getFrameBuffer(640, 480);
	memset(buffer, 0, 640*480*4);
    ndnrtc::FrameInfo phony;
	r.renderBGRAFrame(phony, 640, 480, buffer);

	buffer = r.getFrameBuffer(1280, 720);
	memset(buffer, 0, 1280*720*4);
	r.renderBGRAFrame(phony, 1280, 720, buffer);

	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/client1-camera.1280x720").c_str()).good());
	remove("/tmp/client1-camera.640x480");
	remove("/tmp/client1-camera.1280x720");
}

TEST(TestRenderer, TestThrowOnSinkCreation)
{
    boost::asio::io_service io;
	std::string sinkName = "/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestThrowOnSinkCreation2)
{
    boost::asio::io_service io;
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink, io);
	
	EXPECT_ANY_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression)
{
    boost::asio::io_service io;
	std::string sinkName = "/client1-camera";
	RendererInternal r(sinkName, createNewSink, io, true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestSinkSuppression2)
{
    boost::asio::io_service io;
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink, io, true);
	
	EXPECT_NO_THROW(r.getFrameBuffer(640, 480));
}

TEST(TestRenderer, TestRenderFrameNoSink)
{
    boost::asio::io_service io;
	std::string sinkName = "";
	RendererInternal r(sinkName, createNewSink, io, true);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
    ndnrtc::FrameInfo phony;
	r.renderBGRAFrame(phony, width, height, buffer);

	ASSERT_FALSE(std::ifstream(string("/tmp/client1-camera.640x480").c_str()).good());
}

TEST(TestRenderer, TestWrongArgs)
{
    boost::asio::io_service io;
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	
	uint8_t *buf = r.getFrameBuffer(640, 480);
    ndnrtc::FrameInfo phony;
	EXPECT_ANY_THROW(r.renderBGRAFrame(phony, 1280, 720, buf));
}

TEST(TestRenderer, TestWrongCallSequence)
{
    boost::asio::io_service io;
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	ndnrtc::FrameInfo phony;
	EXPECT_ANY_THROW(r.renderBGRAFrame(phony, 1280, 720, nullptr));
}

TEST(TestRenderer, TestFrameCorrectness)
{
    boost::asio::io_service io;
	std::string sinkName = "/tmp/client1-camera";
	RendererInternal r(sinkName, createNewSink, io);
	unsigned int width = 640, height = 480;

	uint8_t *buffer = r.getFrameBuffer(width, height);
	
	memset(buffer, 0, width*height*4);
	for (int i = 0; i < width*height*4; ++i)
		buffer[i] = (i%256);

    ndnrtc::FrameInfo phony;
	r.renderBGRAFrame(phony, width, height, buffer);

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
