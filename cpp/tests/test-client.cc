// 
// tests-client.cc
//
//  Created by Peter Gusev on 09 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/chrono.hpp>

#include "gtest/gtest.h"
#include "client/src/client.h"
#include "client/src/config.h"
#include "tests-helpers.h"

using namespace ::testing;
using namespace std;
using namespace boost::chrono;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;

TEST(TestClient, TestSingleton)
{
	Client& c = Client::getSharedInstance();
}

TEST(TestClient, TestRunClientPhony)
{
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelNone);

	MockNdnRtcLibrary ndnrtcLib;
	Client& c = Client::getSharedInstance();

	EXPECT_CALL(ndnrtcLib, setObserver(_));

	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	c.run(&ndnrtcLib, 3, 10, ClientParams());
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
    
    auto duration = duration_cast<seconds>( t2 - t1 ).count();

	EXPECT_EQ(3, duration);
}

TEST(TestClient, TestConsumer)
{
	ClientParams cp = sampleConsumerParams();
	// ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);

	MockNdnRtcLibrary ndnrtcLib;
	Client& c = Client::getSharedInstance();

	EXPECT_CALL(ndnrtcLib, setObserver(_));
	EXPECT_CALL(ndnrtcLib, 
		addRemoteStream(cp.getConsumerParams().fetchedStreams_[0].sessionPrefix_,
		cp.getConsumerParams().fetchedStreams_[0].threadToFetch_, _, _, _, _))
			.Times(1)
			.WillOnce(Return(cp.getConsumerParams().fetchedStreams_[0].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[0].streamName_));
	EXPECT_CALL(ndnrtcLib, 
		addRemoteStream(cp.getConsumerParams().fetchedStreams_[1].sessionPrefix_,
		cp.getConsumerParams().fetchedStreams_[1].threadToFetch_, _, _, _, _))
			.Times(1)
			.WillOnce(Return(cp.getConsumerParams().fetchedStreams_[1].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[1].streamName_));
	EXPECT_CALL(ndnrtcLib,
		removeRemoteStream(cp.getConsumerParams().fetchedStreams_[0].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[0].streamName_))
		.Times(1);
	EXPECT_CALL(ndnrtcLib,
		removeRemoteStream(cp.getConsumerParams().fetchedStreams_[1].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[1].streamName_))
		.Times(1);

	boost::shared_ptr<StatisticsStorage> sampleStats = 
		boost::shared_ptr<StatisticsStorage>(StatisticsStorage::createConsumerStatistics());

	EXPECT_CALL(ndnrtcLib, getRemoteStreamStatistics(cp.getConsumerParams().fetchedStreams_[0].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[0].streamName_))
		.Times(AtLeast(28))
		.WillRepeatedly(ReturnPointee(sampleStats));
	EXPECT_CALL(ndnrtcLib, getRemoteStreamStatistics(cp.getConsumerParams().fetchedStreams_[1].sessionPrefix_+"/"+cp.getConsumerParams().fetchedStreams_[1].streamName_))
		.Times(AtLeast(28))
		.WillRepeatedly(ReturnPointee(sampleStats));

	c.run(&ndnrtcLib, 3, 100, cp);

	EXPECT_TRUE(std::ifstream("/tmp/buffer-client1-mic.stat").good());
	EXPECT_TRUE(std::ifstream("/tmp/buffer-client1-camera.stat").good());
	remove("/tmp/buffer-client1-mic.stat");
	remove("/tmp/buffer-client1-camera.stat");
}

// TEST(TestClient, TestThrowsAtBadProducerParams)
// {
// 	ProducerClientParams pcp;
// 	pcp.publishedStreams_.push_back(ProducerStreamParams());

// 	ClientParams cp;
// 	cp.setProducerParams(pcp);

// 	EXPECT_ANY_THROW(
// 		Client::getSharedInstance().run(5, 10, cp)
// 		);
// }

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
