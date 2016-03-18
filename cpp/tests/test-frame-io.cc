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
	boost::shared_ptr<FileSink> sink(new FileSink("/tmp/test-sink.argb"));
	ArgbFrame frame(640, 480);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	*sink << frame;
	sink.reset();

	FILE *f = fopen("/tmp/test-sink.argb", "rb");
	fseek (f , 0 , SEEK_END);
	long lSize = ftell(f);
	rewind (f);

	ASSERT_EQ(lSize, frame.getFrameSizeInBytes());
	uint8_t *buf = (uint8_t*)malloc(sizeof(uint8_t)*lSize);

	fread(buf, sizeof(uint8_t), lSize, f);

	for (int i = 0; i < lSize; ++i)
		ASSERT_EQ(buf[i], (i%256));

	remove("/tmp/test-sink.argb");
}

TEST(TestSink, TestSinkThrowOnCreation)
{
	EXPECT_ANY_THROW(FileSink(""));
	EXPECT_ANY_THROW(FileSink("/test-sink"));
}

TEST(TestSource, TestSourceCheck)
{
	boost::shared_ptr<FileSink> sink(new FileSink("/tmp/test-sink.argb"));
	ArgbFrame frame(640, 480);
	uint8_t *b = frame.getBuffer().get();

	for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
		b[i] = (i%256);

	for (int i = 0; i < 30; i++)
		*sink << frame;
	sink.reset();

	ASSERT_TRUE(FileFrameSource::checkSourceForFrame("/tmp/test-sink.argb", frame));
	remove("/tmp/test-sink.argb");
}

TEST(TestSource, TestSource)
{
	{
		boost::shared_ptr<FileSink> sink(new FileSink("/tmp/test-sink.argb"));
		ArgbFrame frame(640, 480);
		uint8_t *b = frame.getBuffer().get();

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			b[i] = (i%256);

		for (int i = 0; i < 30; i++)
			*sink << frame;
		sink.reset();
	}
	
	FileFrameSource source("/tmp/test-sink.argb");
	ArgbFrame frame(640,480);

	EXPECT_EQ(frame.getFrameSizeInBytes()*30, source.getSize());

	unsigned int frameCount = 0;

	while (!source.isEof())
	{
		source >> frame;
		frameCount ++;

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			EXPECT_EQ((i%256), (frame.getBuffer().get())[i]);
	}

	EXPECT_EQ(30, frameCount);
	EXPECT_ANY_THROW(source >> frame);

	remove("/tmp/test-sink.argb");
}

TEST(TestSource, TestBadSourcePath)
{
	EXPECT_ANY_THROW(FileFrameSource(""));
	EXPECT_ANY_THROW(FileFrameSource("/test-source.argb"));
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
