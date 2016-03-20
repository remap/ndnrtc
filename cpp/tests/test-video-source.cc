// 
// test-video-source.cc
//
//  Created by Peter Gusev on 18 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.h"
#include "client/src/config.h"
#include "client/src/video-source.h"

using namespace std;
using namespace ::testing;
using namespace boost::chrono;

TEST(TestVideoSource, TestBlank)
{
	boost::asio::io_service io;
	EXPECT_ANY_THROW(VideoSource(io, "", boost::shared_ptr<RawFrame>(new ArgbFrame(640, 480))));
}

TEST(TestVideoSource, TestUnappropriateSource)
{
	std::string fname = "/tmp/test-sink1.argb";
	{
		FileSink fs(fname);
		fs << ArgbFrame(640, 480);
	}

	boost::asio::io_service io;
	boost::shared_ptr<ArgbFrame> frame(new ArgbFrame(1280, 800));
	EXPECT_ANY_THROW(VideoSource(io, fname, frame));

	remove(fname.c_str());
}

TEST(TestVideoSource, TestVideoSourcing)
{
	std::string fname = "/tmp/test-sink2.argb";
	{
		FileSink sink(fname);
		ArgbFrame frame(640, 480);
		uint8_t *b = frame.getBuffer().get();

		for (int i = 0; i < frame.getFrameSizeInBytes(); ++i)
			b[i] = (i%256);

		for (int i = 0; i < 1; i++)
			sink << frame;
	}

	Sequence s;
	MockExternalCapturer capturer;

	boost::asio::io_service io;
	boost::shared_ptr<ArgbFrame> frame(new ArgbFrame(640, 480));
	VideoSource vs(io, fname, frame);

	{
		InSequence s;
		EXPECT_CALL(capturer, capturingStarted());
		EXPECT_CALL(capturer, incomingArgbFrame(640, 480, _, frame->getFrameSizeInBytes()))
			.Times(90);
		EXPECT_CALL(capturer, capturingStopped());
	}

	boost::shared_ptr<boost::asio::io_service::work> work;
	boost::asio::deadline_timer runTimer(io);
	high_resolution_clock::time_point t1 = high_resolution_clock::now();

	runTimer.expires_from_now(boost::posix_time::milliseconds(3000));
	runTimer.async_wait([&](const boost::system::error_code& ){
		vs.stop();
		work.reset();
	});
	
	vs.addCapturer(&capturer);
	vs.start(30);

	work.reset(new boost::asio::io_service::work(io));
	io.run();
	io.stop();

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	auto duration = duration_cast<seconds>( t2 - t1 ).count();

	GT_PRINTF("mean time to source 1 frame was %.4f ms\n", vs.getMeanSourcingTimeMs());
	GT_PRINTF("sourced %d frames\n", vs.getSourcedFramesNumber());
	EXPECT_EQ(3, duration);
	EXPECT_EQ(90-1, vs.getRewinds());
	EXPECT_EQ(vs.getSourcedFramesNumber(), 90);

	remove(fname.c_str());
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
