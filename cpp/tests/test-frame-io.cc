// 
// test-frame-io.cc
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "tests/tests-helpers.hpp"
#include "gtest/gtest.h"
#include "client/src/frame-io.hpp"

#ifdef HAVE_NANOMSG
#include "client/src/ipc-shim.h"
#endif

TEST(TestFrame, TestArgFrame)
{
	ArgbFrame frame(640, 480);
	EXPECT_EQ(640*480*4, frame.getFrameSizeInBytes());
	EXPECT_EQ(640*480*4, ArgbFrame(640,480).getFrameSizeInBytes());

	unsigned int w,h;
	frame.getFrameResolution(w,h);
	EXPECT_EQ(640, w);
	EXPECT_EQ(480, h);
}

TEST(TestSink, TestSink)
{
	std::string fname = "/tmp/test-sink1.argb";
	boost::shared_ptr<FileSink> sink(new FileSink(fname));
	ArgbFrame frame(640, 480);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	*sink << frame;
	sink.reset();

	FILE *f = fopen(fname.c_str(), "rb");
	fseek (f , 0 , SEEK_END);
	long lSize = ftell(f);
	rewind (f);

	ASSERT_EQ(lSize, frame.getFrameSizeInBytes());
	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*lSize);

	fread(buf, sizeof(uint8_t), lSize, f);

	for (int i = 0; i < lSize; ++i)
		ASSERT_EQ(buf[i], (i%256));

	remove(fname.c_str());
}

TEST(TestSink, TestSinkThrowOnCreation)
{
	EXPECT_ANY_THROW(FileSink(""));
	EXPECT_ANY_THROW(FileSink("/test-sink"));
}

TEST(TestSource, TestSourceCheck)
{
	std::string fname = "/tmp/test-sink2.argb";
	boost::shared_ptr<FileSink> sink(new FileSink(fname));
	ArgbFrame frame(640, 480);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	for (int i = 0; i < 30; i++)
		*sink << frame;
	sink.reset();

	ASSERT_TRUE(FileFrameSource::checkSourceForFrame(fname, frame));
	remove(fname.c_str());
}

TEST(TestSource, TestSource)
{
	std::string fname = "/tmp/test-sink3.argb";
	{
		boost::shared_ptr<FileSink> sink(new FileSink(fname));
		ArgbFrame frame(640, 480);
		uint8_t *b = frame.getBuffer().get();

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			b[i] = (i%256);

		for (int i = 0; i < 30; i++)
			*sink << frame;
		sink.reset();
	}
	
	FileFrameSource source(fname);
	ArgbFrame frame(640,480);

	EXPECT_EQ(frame.getFrameSizeInBytes()*30, source.getSize());

	unsigned int frameCount = 0;

	do
	{
		source >> frame;
		if (!source.isEof()) frameCount++;

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			EXPECT_EQ((i%256), (frame.getBuffer().get())[i]);

	} while (!source.isEof());

	EXPECT_EQ(30, frameCount);

	remove(fname.c_str());
}

TEST(TestSource, TestRewind)
{
	std::string fname = "/tmp/test-sink4.argb";
	{
		boost::shared_ptr<FileSink> sink(new FileSink(fname));
		ArgbFrame frame(640, 480);
		uint8_t *b = frame.getBuffer().get();

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			b[i] = (i%256);

		for (int i = 0; i < 2; i++)
			*sink << frame;
		sink.reset();
	}
	
	FileFrameSource source(fname);
	ArgbFrame frame(640,480);

	unsigned int frameCount = 0;

	for (int i = 0; i < 30; i++)
	{
		source >> frame;
		if (source.isEof())
			source.rewind();

		frameCount ++;

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			EXPECT_EQ((i%256), (frame.getBuffer().get())[i]);
	}

	EXPECT_EQ(30, frameCount);

	remove(fname.c_str());
}

TEST(TestSource, TestBadSourcePath)
{
	EXPECT_ANY_THROW(FileFrameSource(""));
	EXPECT_ANY_THROW(FileFrameSource("/test-source.argb"));
}

TEST(TestPipeSink, TestCreate)
{
	std::string fname = "/tmp/test-pipe.argb";
	boost::shared_ptr<PipeSink> sink(new PipeSink(fname));

	ASSERT_TRUE( access( fname.c_str(), F_OK ) != -1 );

	remove(fname.c_str());
}

TEST(TestPipeSink, TestCreateFailure)
{
	std::string fname = "/etc/test-pipe.argb";
	EXPECT_ANY_THROW(
		boost::shared_ptr<PipeSink> sink(new PipeSink(fname))
		);
}

TEST(TestPipeSink, TestWriteToNonOpen)
{
	std::string fname = "/tmp/test-pipe.argb";
	boost::shared_ptr<PipeSink> sink(new PipeSink(fname));
	ArgbFrame frame(640, 480);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	EXPECT_NO_THROW(*sink << frame);

	remove(fname.c_str());
}

TEST(TestPipeSink, TestWriteAndRead)
{
	std::string fname = "/tmp/test-pipe.argb";
	boost::shared_ptr<PipeSink> sink(new PipeSink(fname));
	ArgbFrame frame(1280, 720);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	boost::thread t([&sink, &frame]{
		do {
			EXPECT_NO_THROW(*sink << frame);
		} while(!sink->isLastWriteSuccessful() && !sink->isBusy());
	});

	int pipe = open(fname.c_str(), O_RDONLY);
	ASSERT_TRUE(pipe > 0);

	size_t bufSz = frame.getFrameSizeInBytes();
	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*bufSz);
	int readBytes = 0;
	do {
		int r = read(pipe, buf+readBytes, bufSz-readBytes);
		if (r > 0) readBytes += r;
	} while (readBytes != bufSz);

	EXPECT_TRUE(sink->isLastWriteSuccessful());
	t.join(); 
	sink.reset();

	remove(fname.c_str());
}

#ifdef HAVE_NANOMSG
// TEST(TestNanoSink, TestWriteAndRead)
// {
// 	std::string handle = "testnano";
// 	boost::shared_ptr<NanoMsgSink> sink;
// 	EXPECT_NO_THROW(sink.reset(new NanoMsgSink(handle)));

// 	ArgbFrame frame(1280, 720);
// 	uint8_t *b = frame.getBuffer().get();

// 	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
// 		b[i] = (i%256);

// 	boost::thread t([&sink, &frame]{
// 		EXPECT_NO_THROW(*sink << frame);
// 		std::cout << "wrote" << std::endl;
// 		EXPECT_TRUE(sink->isLastWriteSuccessful());
// 	});

// 	int socket = ipc_setupSubSinkSocket(handle.c_str());
// 	ASSERT_TRUE(socket >= 0);

// 	size_t bufSz = frame.getFrameSizeInBytes();
// 	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*bufSz);
// 	int readBytes = 0;
	
// 	int r = ipc_readData(socket, buf, bufSz);

// 	ASSERT_EQ(r, bufSz);
// 	EXPECT_TRUE(sink->isLastWriteSuccessful());

// 	t.join(); 
// 	sink.reset();
// }
#endif

#if 0
TEST(TestSource, TestTemp)
{
	std::string fname = "/tmp/clientA-camera-0-320x180.argb";
	FileFrameSource source(fname);
	ArgbFrame frame(320,180);
	unsigned int frameCount = 0;
	boost::shared_ptr<FileSink> sink(new FileSink("/tmp/test-320x180-30frames.argb"));

	for (int i = 0; i < 30; i++)
	{
		do
		{
			if (source.isEof()) source.rewind();
			source >> frame;
		} while (source.isEof());

		*sink << frame;
	}

	sink.reset();
}
#endif

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
