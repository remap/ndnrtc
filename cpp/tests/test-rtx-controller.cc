// 
// test-rtx-controller.cc
//
//  Created by Peter Gusev on 23 August 2017.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <algorithm>
#include <ctime>

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/name.hpp>
#include <boost/shared_ptr.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"

#include "mock-objects/rtx-observer-mock.hpp"
#include "mock-objects/buffer-mock.hpp"
#include "mock-objects/playback-queue-mock.hpp"

#include "statistics.hpp"
#include "src/frame-data.hpp"
#include "src/frame-buffer.hpp"
#include "src/rtx-controller.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace testing;

TEST(TestRtxController, TestRtx){
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());

	MockRtxObserver rtxObserverMock;
	RetransmissionController rtx = RetransmissionController(storage, playbackQueue);
	rtx.attach(&rtxObserverMock);

	int nPackets = 10;
	std::srand(std::time(0));
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d";
	IBufferObserver *bufferObserver = &rtx;

	std::set<Name> rtxInterestNames;
	EXPECT_CALL(rtxObserverMock, onRetransmissionRequired(_))
		.Times(AtLeast(nPackets/2))
		.WillRepeatedly(Invoke([&rtxInterestNames](const std::vector<boost::shared_ptr<const ndn::Interest>> interests){
			GT_PRINTF("RTX for %d interests\n", interests.size());

			for (auto i:interests)
			{
				if (rtxInterestNames.find(i->getName()) != rtxInterestNames.end())
					FAIL() << "RTX for already retransmitted interest";
				rtxInterestNames.insert(i->getName());
			}
		}));

	for (int i = 0; i < nPackets; ++i)
	{
		std::vector<boost::shared_ptr<const Interest>> interests;
		int nInterests = std::rand()%30+10;
		for (int j = 0; j < nInterests; ++j)
		{
			boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(frameName).appendSequenceNumber(i).appendSegment(j), 1000));
			int nonce = 0x1234+i*10+j;
			interest->setNonce(Blob((uint8_t*)&nonce, sizeof(int)));
			interests.push_back(interest);
		}

		boost::shared_ptr<BufferSlot> slot = boost::make_shared<BufferSlot>();
		slot->segmentsRequested(interests);

		EXPECT_CALL(*playbackQueue, size())
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke([](){ return 100; }));
		EXPECT_CALL(*playbackQueue, pendingSize())
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke([](){ return 100; }));

		bufferObserver->onNewRequest(slot);

		usleep(33000);
	}
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
