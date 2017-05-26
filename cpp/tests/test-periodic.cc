// 
// test-periodic.cc
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/make_shared.hpp>

#include "gtest/gtest.h"
#include "src/periodic.hpp"

using namespace ndnrtc;

class PeriodicTest : public Periodic, public boost::enable_shared_from_this<PeriodicTest>
{
public:
	PeriodicTest(boost::asio::io_service& io, unsigned int periodMs):
		Periodic(io), periodMs_(periodMs), workCounter_(0)
		{}

	~PeriodicTest(){
	}

	unsigned int periodicInvocation() {
		boost::lock_guard<boost::mutex> scopedLock(mutex_);
		workCounter_++;
		return periodMs_;
	}

	boost::mutex mutex_;
	unsigned int periodMs_;
	unsigned int workCounter_;
};

TEST(TestPeriodic, TestDestructionOnDifferentThread)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io));
	boost::thread t([&io](){
		io.run();
	});
	unsigned int workCounter = 0;
	unsigned int d = 50;

	{
		boost::shared_ptr<PeriodicTest> p(boost::make_shared<PeriodicTest>(io, d));
		p->setupInvocation(d, boost::bind(&PeriodicTest::periodicInvocation, p));
		boost::this_thread::sleep_for(boost::chrono::milliseconds(540));
		workCounter = p->workCounter_;
		p->cancelInvocation();
	}
	work.reset();
	t.join();

	EXPECT_EQ(10, workCounter);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
