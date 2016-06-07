// 
// test-periodic.cc
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/thread.hpp>

#include "gtest/gtest.h"

#include "src/periodic.h"

using namespace ndnrtc;

class PeriodicTest : public Periodic
{
public:
	PeriodicTest(boost::asio::io_service& io, unsigned int periodMs):
		Periodic(io, periodMs), periodMs_(periodMs), workCounter_(0){}
	~PeriodicTest(){
	}

	unsigned int periodicInvocation() {
		workCounter_++;
		return periodMs_;
	}

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

	{
		PeriodicTest p(io, 50);
		boost::this_thread::sleep_for(boost::chrono::milliseconds(550));
		workCounter = p.workCounter_;
	}
	work.reset();
	t.join();

	EXPECT_EQ(10, workCounter);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
