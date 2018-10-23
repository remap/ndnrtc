// 
// test-video-source.cc
//
//  Created by Peter Gusev on 18 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "mock-objects/external-capturer-mock.hpp"
#include "client/src/config.hpp"
#include "client/src/video-source.hpp"
#include "estimators.hpp"

using namespace std;
using namespace ::testing;
using namespace boost::chrono;
using namespace ndnrtc::estimators;

std::string test_path = "";

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
		EXPECT_CALL(capturer, incomingArgbFrame(640, 480, _, frame->getFrameSizeInBytes()))
			.Times(AtLeast(88));
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
	EXPECT_LE(88, vs.getRewinds());
	EXPECT_GE(vs.getSourcedFramesNumber(), 88);

	remove(fname.c_str());
}

TEST(TestVideoSource, TestHdVideoSourcing)
{
    std::string fname = test_path+"../../res/test-source-1280x720.argb";
    Sequence s;
    MockExternalCapturer capturer;
    
    boost::asio::io_service io;
    boost::shared_ptr<ArgbFrame> frame(new ArgbFrame(1280, 720));
    VideoSource vs(io, fname, frame);
    
    Average avg(boost::make_shared<TimeWindow>(500));
    high_resolution_clock::time_point captureTp;
    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
    incomingFrame = [&captureTp, &avg](const unsigned int width,
                       const unsigned int height,
                       unsigned char* argbFrameData,
                       unsigned int frameSize){
        high_resolution_clock::time_point now = high_resolution_clock::now();
        
        if (captureTp != high_resolution_clock::time_point())
        {
            auto duration = duration_cast<milliseconds>( now - captureTp).count();
            avg.newValue((double)duration);
        }
        
        captureTp = now;
        return 0;
    };
    
    EXPECT_CALL(capturer, incomingArgbFrame(1280, 720, _, _))
        .Times(90)
        .WillRepeatedly(Invoke(incomingFrame));
    
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
    GT_PRINTF("mean sample period: %.2f, var: %.2f, dev: %.2f\n",
              avg.value(), avg.variance(), avg.deviation());
    
    EXPECT_EQ(3, duration);
    EXPECT_LE(fabs(90-vs.getSourcedFramesNumber()), 1);
}


//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
    
    test_path = std::string(argv[0]);
    std::vector<std::string> comps;
    boost::split(comps, test_path, boost::is_any_of("/"));
    
    test_path = "";
    for (int i = 0; i < comps.size()-1; ++i)
    {
        test_path += comps[i];
        if (i != comps.size()-1) test_path += "/";
    }

	return RUN_ALL_TESTS();
}
