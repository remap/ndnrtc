// 
// test-interest-control.cc
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <ctime>
#include <boost/chrono.hpp>

#include "gtest/gtest.h"
#include "interest-control.hpp"
#include "drd-estimator.hpp"
#include "statistics.hpp"

#include "tests-helpers.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

// #define ENABLE_LOGGING

TEST(TestInterestControl, TestDefault)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	InterestControl ictrl(drd, storage);
	drd->attach(&ictrl);

#ifdef ENABLE_LOGGING
	ictrl.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	int oneWayDelay = 75;
	int deviation = 5;
	int nSegments = 100;
	std::srand(std::time(0));
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	int keySeqNo = 0, deltaSeqNo = 0;
	int gop = 30, fps = 30;

	// originally - pipeline 3, after DRD update - 5
	// 0:  5 samples - burst (pipeline 8) true
	// 1: 10 samples - withhold (pipeline 6) true
	// 2: 15 samples - withhold (pipeline 5) true
	// 3: 20 samples - withhold (pipeline 5) false

	EXPECT_EQ(3, ictrl.pipelineLimit());
	EXPECT_EQ(0, ictrl.pipelineSize());
	EXPECT_EQ(3, ictrl.room());

	DelayQueue queue(io, oneWayDelay, deviation);

	// update DRD and set target rate after getting initial data
	int drdValue = (std::rand()%(deviation/2)-deviation/2) +(2*oneWayDelay);
	drd->newValue(drdValue, true, 0);
	ictrl.targetRateUpdate(fps);

	EXPECT_EQ(5, ictrl.pipelineLimit());
	EXPECT_EQ(0, ictrl.pipelineSize());
	EXPECT_EQ(5, ictrl.room());

	int nReceived = 0;
	int ppChange = 0;

	for (int i = 0; i < nSegments; ++i)
	{
		while (ictrl.room() <= 0) ;

		int shouldExpress = ictrl.room();
		int n = i;
		while (ictrl.increment() && i < nSegments)
		{
			i++;
			boost::chrono::high_resolution_clock::time_point t = boost::chrono::high_resolution_clock::now();
			queue.push([&queue, &ictrl, &drd, &nReceived, &ppChange, t](){
				queue.push([&ictrl, &drd, &nReceived, &ppChange, t](){
					drd->newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::high_resolution_clock::now()-t).count(), true, 0);
					EXPECT_TRUE(ictrl.decrement());
					nReceived++;
					if (nReceived%5 == 0)
					{
						if (ppChange == 0)
						{
							ictrl.burst();
							EXPECT_EQ(8, ictrl.pipelineLimit());
						}
						else
						{
							bool res = ictrl.withhold();
							if (ppChange < 3)
							{
								EXPECT_TRUE(res);
								EXPECT_EQ(8-ppChange-1, ictrl.pipelineLimit());
							}
							else
							{
								EXPECT_FALSE(res);
								EXPECT_EQ(5, ictrl.pipelineLimit());
							}
						}
						ppChange++;
					}
				});
			});
		} // while
	}

	work.reset();
	t.join();

	EXPECT_EQ(5, ictrl.room());
}

TEST(TestInterestControl, TestViolationsOfLowerBoundary)
{
	{
		boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150, 1));
        boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
		InterestControl ictrl(drd,storage);
		drd->attach(&ictrl);

		drd->newValue(75, true, 0);
		ictrl.targetRateUpdate(30.);
		ictrl.burst();
		EXPECT_TRUE(ictrl.withhold());
		drd->newValue(75, true, 0);
		EXPECT_TRUE(ictrl.withhold());
	}
	{
		boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150, 1));
        boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
		InterestControl ictrl(drd, storage);
		drd->attach(&ictrl);

		drd->newValue(75, true, 0);
		ictrl.targetRateUpdate(30.);
		
		EXPECT_EQ(3, ictrl.room());
		ictrl.markLowerLimit(5);
		EXPECT_EQ(5, ictrl.room());
		EXPECT_FALSE(ictrl.withhold());
		drd->newValue(75, true, 0);
		EXPECT_FALSE(ictrl.withhold());
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
