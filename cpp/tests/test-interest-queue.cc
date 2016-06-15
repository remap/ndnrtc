// 
// test-interest-queue.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include "gtest/gtest.h"
#include "interest-queue.h"

#include "mock-objects/interest-queue-observer-mock.h"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace ::testing;

// #define ENABLE_LOGGING

TEST(TestInterestQueue, TestDefault)
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
	
	boost::shared_ptr<ndn::ThreadsafeFace> face(boost::make_shared<ndn::ThreadsafeFace>(io, "aleph.ndn.ucla.edu"));
	boost::shared_ptr<statistics::StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());

	int n = 100;
	MockInterestQueueObserver o;
	InterestQueue iq(io, face, storage);
	iq.registerObserver(&o);
#ifdef ENABLE_LOGGING
	iq.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	OnData onData = [](const boost::shared_ptr<const ndn::Interest>&,
                                    const boost::shared_ptr<ndn::Data>&){
		ASSERT_FALSE(true);
	};

	int nTimeouts = 0;
	OnTimeout onTimeout = [&nTimeouts](const boost::shared_ptr<const ndn::Interest>& i){
		nTimeouts++;
	};

	EXPECT_CALL(o, onInterestIssued(_))
		.Times(n);

	for (int i = 0; i < n; ++i)
	{
		std::stringstream ss;
		ss << "/timeout/" << i;
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(ss.str(), 1000));
		iq.enqueueInterest(interest, DeadlinePriority::fromNow(200), onData, onTimeout);
	}

	while (iq.size()) 
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

	work.reset();
	io.stop();
	t.join();

	EXPECT_EQ(n, nTimeouts);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
