// 
// test-drd-estimator.cc
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>

#include "gtest/gtest.h"
#include "src/drd-estimator.hpp"
#include "tests-helpers.hpp"

#include "mock-objects/drd-estimator-observer-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;

TEST(TestDrdEstimator, TestDefault)
{
	MockDrdEstimatorObserver o;
	DrdEstimator drd;
	drd.attach(&o);
	EXPECT_EQ(150, drd.getOriginalEstimation());
	EXPECT_EQ(150, drd.getCachedEstimation());

	EXPECT_CALL(o, onOriginalDrdUpdate(_,_))
		.Times(100);
	EXPECT_CALL(o, onCachedDrdUpdate(_,_))
		.Times(100);
	EXPECT_CALL(o, onDrdUpdate())
		.Times(200);

	for (int i = 1; i <= 100; ++i)
		drd.newValue(i, true, 0);

	for (double i = 1; i <= 50.5; i+=0.5)
		drd.newValue(i, false, 0);

	EXPECT_EQ(50.5, drd.getOriginalEstimation());
	EXPECT_EQ(25.75, drd.getCachedEstimation());
	EXPECT_EQ(25.75, drd.getLatestUpdatedAverage().value());

	drd.reset();
	EXPECT_EQ(150, drd.getOriginalEstimation());
	EXPECT_EQ(150, drd.getCachedEstimation());
}

TEST(TestDrdEstimator, TestSetInitialValue)
{
    DrdEstimator drd;

    EXPECT_EQ(150, drd.getOriginalEstimation());
    EXPECT_EQ(150, drd.getCachedEstimation());

    drd.setInitialEstimation(180);

    EXPECT_EQ(180, drd.getOriginalEstimation());
    EXPECT_EQ(180, drd.getCachedEstimation());
}

TEST(TestDrdEstimator, TestDrdDeviation)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int oneWayDelay = 75;
	int deviation = 0;
	double drdError = 0.1;
	int deviationInterval = 10;
	double devError = 0.3;

	while (deviation < oneWayDelay)
	{
		DelayQueue queue(io, oneWayDelay, deviation);
		DrdEstimator drd(150, 1000);
	
		int n = 500;
		for (int i = 0; i < n; ++i)
		{
			boost::chrono::high_resolution_clock::time_point issue = boost::chrono::high_resolution_clock::now();
	
			queue.push([&queue, &drd, issue](){
				queue.push([&drd, issue](){
					boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
					drd.newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count(), true, 0);
				});
			});
	
			boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
		}
	
		while (queue.size())
			boost::this_thread::sleep_for(boost::chrono::milliseconds(2*oneWayDelay));
	
		GT_PRINTF("1-way delay: %dms (deviation %dms); measured drd: %.2fms, dev: %.2f (%.2f%%)\n",
			oneWayDelay, deviation, drd.getLatestUpdatedAverage().value(), 
			drd.getLatestUpdatedAverage().deviation(),
                  drd.getLatestUpdatedAverage().deviation()/drd.getLatestUpdatedAverage().value()*100);
		EXPECT_LE(std::abs(oneWayDelay*2-drd.getLatestUpdatedAverage().value())/((double)oneWayDelay*2), drdError);
		if (deviation)
			EXPECT_LE(std::abs((float)(deviation-drd.getLatestUpdatedAverage().deviation()))/(double)deviation, devError);

		deviation += deviationInterval;
	}

	work.reset();
	t.join();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
