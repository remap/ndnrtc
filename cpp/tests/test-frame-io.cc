// 
// test-frame-io.cc
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "tests/tests-helpers.h"
#include "gtest/gtest.h"
#include "client/src/frame-io.h"

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
