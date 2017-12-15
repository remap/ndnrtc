// 
// test-interest-queue.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <bitset>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include "gtest/gtest.h"
#include "interest-queue.hpp"
#include "tests-helpers.hpp"

#include "mock-objects/interest-queue-observer-mock.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace ::testing;

// #define ENABLE_LOGGING

TEST(TestInterestQueue, TestDefault)
{
	ASSERT_TRUE(checkNfd()) << "Apparently, local NFD is not running. Aborting test.";

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});
	
	boost::shared_ptr<ndn::ThreadsafeFace> face(boost::make_shared<ndn::ThreadsafeFace>(io));
	boost::shared_ptr<statistics::StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());

	int n = 10, nSegments = 10;
	MockInterestQueueObserver o;
	InterestQueue iq(io, face, storage);
	iq.registerObserver(&o);
#ifdef ENABLE_LOGGING
	iq.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	OnData onData = [](const boost::shared_ptr<const ndn::Interest>&,
                                    const boost::shared_ptr<ndn::Data>&){
		ASSERT_FALSE(true);
	};
	
	int nTimeouts = 0;
	int lastSeqNo = 0, lastSegNo = -1;
	OnTimeout onTimeout = [&nTimeouts, &lastSeqNo, &lastSegNo, nSegments](const boost::shared_ptr<const ndn::Interest>& i){
		nTimeouts++;
	};

	std::bitset<100> iBitSet(0);
	EXPECT_CALL(o, onInterestIssued(_))
		.Times(n*nSegments)
		.WillRepeatedly(Invoke([&iBitSet](const boost::shared_ptr<const ndn::Interest>& i){
			int bitNo = i->getName()[-2].toSequenceNumber()*10+i->getName()[-1].toSegment();
			
			EXPECT_FALSE(iBitSet[bitNo]);
			iBitSet[bitNo] = 1;
		}));

	for (int i = 0; i < n; ++i)
	{
		Name n("/timeout");

		for (int j = 0; j < nSegments; ++j)
		{
			boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name("/timeout").appendSequenceNumber(i).appendSegment(j), 1000));
			iq.enqueueInterest(interest, DeadlinePriority::fromNow(200), onData, onTimeout);
		}
	}

	while (iq.size()) 
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

	work.reset();
	io.stop();
	t.join();

	EXPECT_EQ(n*nSegments, nTimeouts);
}

TEST(TestInterestQueue, TestNacks)
{
	ASSERT_TRUE(checkNfd()) << "Apparently, local NFD is not running. Aborting test.";

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});
	
	boost::shared_ptr<ndn::ThreadsafeFace> face(boost::make_shared<ndn::ThreadsafeFace>(io));
	boost::shared_ptr<statistics::StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());

	int n = 100;
	MockInterestQueueObserver o;
	InterestQueue iq(io, face, storage);
	iq.registerObserver(&o);
#ifdef ENABLE_LOGGING
	iq.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	OnData onData = [](const boost::shared_ptr<const ndn::Interest>&,
                                    const boost::shared_ptr<ndn::Data>&){
		ASSERT_FALSE(true);
	};

	int nNacks = 0;
	OnNetworkNack onNack = [&nNacks](const boost::shared_ptr<const ndn::Interest>& interest,
        const boost::shared_ptr<ndn::NetworkNack>& networkNack){
		nNacks++;
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
		ss << "/nack/" << i;
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(ss.str(), 1000));
		iq.enqueueInterest(interest, DeadlinePriority::fromNow(200), onData, onTimeout, onNack);
	}

	while (iq.size()) 
		boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

	work.reset();
	io.stop();
	t.join();

	EXPECT_EQ(n, nNacks);
	EXPECT_EQ(0, nTimeouts);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
