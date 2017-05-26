// 
// test-generator.cc
//
//  Created by Peter Gusev on 19 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "precise-generator.hpp"

using namespace std;
using namespace ::testing;
using namespace boost::chrono;

class GeneratorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  void testRate(double rate, unsigned int durationSec, 
  	const PreciseGenerator::Task& task)
  {
  	boost::asio::io_service io;
  	unsigned int count = 0;

  	PreciseGenerator g(io, rate, task);

  	boost::shared_ptr<boost::asio::io_service::work> work;
  	boost::asio::deadline_timer runTimer(io);
  	high_resolution_clock::time_point t1 = high_resolution_clock::now();
  	unsigned int d = durationSec;

  	runTimer.expires_from_now(boost::posix_time::milliseconds(d*1000+10));
  	runTimer.async_wait([&](const boost::system::error_code& ){
  		work.reset();
  		g.stop();
  	});

  	g.start();

  	work.reset(new boost::asio::io_service::work(io));
  	io.run();
  	io.stop();

  	// cout << "mean processing overhead " << fixed << setprecision(2) << g.getMeanProcessingOverheadNs() << " ns" << endl;
  	// cout << "mean task time " << fixed << setprecision(2) << g.getMeanTaskTimeNs() << " ns" << endl;

  	high_resolution_clock::time_point t2 = high_resolution_clock::now();
  	actualDuration_ = duration_cast<seconds>( t2 - t1 ).count();
  	actualTaskDurationNs_ = g.getMeanTaskTimeNs();
  }

  long long actualDuration_;
  double actualTaskDurationNs_;
};

TEST_F(GeneratorTest, Test30FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 30.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test29FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 29.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test28_5FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 28.5;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_GE(count, floor((unsigned int)(actualDuration_)*rate));
	EXPECT_LE(count, ceil((unsigned int)(actualDuration_)*rate));
}

TEST_F(GeneratorTest, Test28FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 28.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test27FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 27.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test25FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 29.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test24_9FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 24.9;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_GE(count, floor((unsigned int)(actualDuration_)*rate));
	EXPECT_LE(count, ceil((unsigned int)(actualDuration_)*rate));
}

TEST_F(GeneratorTest, Test15FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 15.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test10FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 10.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test7FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 7.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test2FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 2.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test1FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 1.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test0FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 1.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test60FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 60.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test300FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 1.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	EXPECT_EQ((unsigned int)(actualDuration_)*rate, count);
}

TEST_F(GeneratorTest, Test500FPS)
{
	unsigned int count = 0;
	unsigned int d = 1;
	double rate = 500.;

	testRate(rate, d, [&count](){
		count++;
		for (int i = 0; i < 1000; ++i); // burn some time
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	// there is 1% positive error when rates are too high
	int error = int(0.01*(d*rate));
	EXPECT_GE(count, floor((unsigned int)(actualDuration_)*rate));
	EXPECT_LE(count, ceil((unsigned int)(actualDuration_)*rate)+error);
}

TEST_F(GeneratorTest, TestLongTask)
{
	unsigned int count = 0;
	unsigned int d = 3;
	double rate = 30.;
	unsigned int taskDurationMs = 40;

	testRate(rate, d, [&count, taskDurationMs](){
		count++;
		boost::this_thread::sleep_for(boost::chrono::milliseconds(taskDurationMs));
	});

	EXPECT_EQ((unsigned int)actualDuration_, d);
	double actualRate = 1000000000./(double)actualTaskDurationNs_;
	int error = int(0.01*(d*actualRate));
	GT_PRINTF("actual count %d (+-%d). actualRate %.4f (vs %,4f requested). task mean time %.4f ms\n", 
		count, error, actualRate, rate, actualTaskDurationNs_/1000000.);
	EXPECT_GE(count, floor((unsigned int)(actualDuration_)*actualRate-error));
	EXPECT_LE(count, ceil((unsigned int)(actualDuration_)*actualRate+error));
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
