// 
// test-buffer-control.cc
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "buffer-control.hpp"
#include "drd-estimator.hpp"
#include "frame-buffer.hpp"
#include "statistics.hpp"

#include "tests-helpers.hpp"
#include "mock-objects/buffer-control-observer-mock.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ::testing;
using namespace ndn;

TEST(TestBufferControl, TestDrdAndLatControlCallbacks)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int nSamples = 100;
	double fps = 30;
	int oneWayDelay = 75;
	int deviation = 30;
	int defPipeline = 5;
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	boost::atomic<int> sampleNo(0);

	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(150));
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150, 500));
	MockBufferControlObserver latControlMock;

    boost::shared_ptr<StatisticsStorage> sstorage(StatisticsStorage::createConsumerStatistics());
	BufferControl bufferControl(drd, buffer, sstorage);
	bufferControl.attach(&latControlMock);

	boost::thread requestor([&sampleNo, threadPrefix, buffer, nSamples, fps](){
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		for (sampleNo = 0; sampleNo < nSamples; ++sampleNo)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds((int)(1000./fps)));

			int nSeg = (sampleNo%((int)fps) == 0 ? 30: 10);
			Name frameName(threadPrefix);
			if (sampleNo%(int)fps == 0)
			{
				frameName.append(NameComponents::NameComponentKey);
			}
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(sampleNo);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			EXPECT_TRUE(buffer->requested(makeInterestsConst(interests)));
			runTimer.wait();
		}
	});

	boost::thread provider([&sampleNo, defPipeline, threadPrefix, &bufferControl, nSamples, 
		fps, &latControlMock]()
	{
		while (sampleNo < defPipeline) ;
		int64_t ts = 488589553, uts = 1460488589;
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		for (int n = 0; n < nSamples; ++n){
			runTimer.expires_from_now(boost::posix_time::milliseconds((int)(1000./fps)));

			Name frameName(threadPrefix);
			if (n%30 == 0)
				frameName.append(NameComponents::NameComponentKey);
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(n);

			VideoFramePacket vp = getVideoFramePacket((n%(int)fps == 0 ? 28000 : 8000),
				fps, ts+n*(int)(1000./fps), uts+n*(int)(1000./fps));
			std::vector<VideoFrameSegment> segments = sliceFrame(vp);
			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, segments.size());
			std::vector<boost::shared_ptr<Data>> data = dataFromSegments(frameName.toUri(), segments);

			EXPECT_CALL(latControlMock, targetRateUpdate(_))
				.Times(1);
			EXPECT_CALL(latControlMock, sampleArrived(_))
				.Times(1);

			int idx = 0;
			for (auto d:data)
			{
				bufferControl.segmentArrived(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx++]));
			}

			runTimer.wait();
		}
	});

	requestor.join();
	provider.join();
	
	GT_PRINTF("DRD estimation: %.2f, dev: %.2f\n",
		drd->getLatestUpdatedAverage().value(), drd->getLatestUpdatedAverage().deviation());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
