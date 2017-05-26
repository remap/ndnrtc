// 
// test-estimators.cc
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>
#include <boost/assign.hpp>

#include "gtest/gtest.h"
#include "src/estimators.hpp"
#include "client/src/precise-generator.hpp"

using namespace ndnrtc::estimators;

TEST(TestEstimatorWindow, TestSampleWindow)
{
	SampleWindow w(10);
	unsigned int nReached = 0, nNotReached = 0;

	for (int i = 0; i < 100; ++i)
		if (w.isLimitReached())
			nReached++;
		else
			nNotReached++;
	EXPECT_EQ(10, nReached);
	EXPECT_EQ(90, nNotReached);
}

TEST(TestEstimatorWindow, TestTimeWindow)
{
	unsigned int interval = 10;
	unsigned int nReached = 0, nNotReached = 0;

	boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
	boost::thread t([interval, &nReached, &nNotReached](){
		TimeWindow w(100);
		for (int i = 0; i < 100; ++i)
		{
			if (w.isLimitReached()) nReached++;
			else nNotReached++;

			boost::this_thread::sleep_for(boost::chrono::milliseconds(interval));
		}
	});

	t.join();
	boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
	auto duration = boost::chrono::duration_cast<boost::chrono::milliseconds>( t2 - t1 ).count();

	EXPECT_EQ(duration/100, nReached);
	EXPECT_EQ(100-duration/100, nNotReached);
}

TEST(TestSlidingAverage, TestSamplesWindow)
{
	Average avg(boost::make_shared<SampleWindow>(10));

	double v = 0;
	int d = 1;
	for (int i = 1; i <= 100; ++i)
	{
		avg.newValue(v);
		if (i%10 == 0) d = -d;
		else v+=d;
	}

	EXPECT_EQ(4.5, avg.value());
	EXPECT_EQ(8.25, avg.variance());
	EXPECT_LT(2.87 - avg.deviation(), 0.01);
}

TEST(TestSlidingAverage, TestTimeWindow)
{
	Average avg(boost::make_shared<TimeWindow>(500));
	boost::thread t([&avg](){
		double v = 0;
		int d = 1;
		for (int i = 1; i <= 100; ++i)
		{
			avg.newValue(v);
			if (i%10 == 0) d = -d;
			else v+=d;
			boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
		}
	});
	t.join();

	EXPECT_LT(abs(4.5 - avg.value()), 1) ;
	EXPECT_LT(abs(8.25-avg.variance()), 1);
	EXPECT_LT(2.87 - avg.deviation(), 0.5);
}

TEST(TestSlidingAverage, TestEdgeValues)
{
    Average avg(boost::make_shared<SampleWindow>(10));
    
    EXPECT_EQ(0, avg.oldestValue());
    EXPECT_EQ(0, avg.latestValue());
    
    double v = 0;
    int d = 1;
    for (int i = 1; i <= 100; ++i)
    {
        avg.newValue(v);
        if (i%10 == 0) d = -d;
        else v+=d;
    }
    
    EXPECT_EQ(4.5, avg.value());
    EXPECT_EQ(8.25, avg.variance());
    EXPECT_LT(2.87 - avg.deviation(), 0.01);
}

TEST(TestFrequencyMeter, TestTimeWindow)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io));
	boost::thread t([&io](){
		io.run();
	});
    
    // smaller windows give less precise results
	FreqMeter freqMeter(boost::make_shared<TimeWindow>(500));
    double rate = 30;
	PreciseGenerator p(io, rate, [&freqMeter](){
		freqMeter.newValue(0);
	});

	p.start();
	boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
	p.stop();
	work.reset();
	t.join();

	EXPECT_LT(abs(rate-freqMeter.value())/rate, 0.15);
}

TEST(TestFrequencyMeter, TestSampleWindow)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io));
	boost::thread t([&io](){
		io.run();
	});

    // smaller windows give less precise results
	FreqMeter freqMeter(boost::make_shared<SampleWindow>(10));
    double rate = 30;
	PreciseGenerator p(io, rate, [&freqMeter](){
		freqMeter.newValue(0);
	});

	p.start();
	boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
	p.stop();
	work.reset();
	t.join();
    
	EXPECT_LT(abs(rate-freqMeter.value())/rate, 0.15);
}

TEST(TestFilter, TestSmoothing)
{
	Filter f(0.05);
	std::vector<double> values = boost::assign::list_of (5.) (6.) (7.) (5.) (10.) (6.) (5.5) (6.1);
	for (auto v:values) f.newValue(v);
	EXPECT_LT(5.5-f.value(), 0.5);
}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
