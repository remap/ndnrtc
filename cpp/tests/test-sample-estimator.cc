// 
// test-sample-estimator.cc
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <ctime>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"

#include "src/sample-estimator.hpp"
#include "src/frame-data.hpp"
#include "statistics.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

std::vector<boost::shared_ptr<WireSegment>> getSegments(unsigned int frameSize, bool isDelta = true)
{
	VideoFramePacket p = getVideoFramePacket(frameSize);
	std::vector<VideoFrameSegment> dataSegments = sliceFrame(p);
	boost::shared_ptr<NetworkData> parityData;
	std::vector<VideoFrameSegment> paritySegments = sliceParity(p, parityData);
	std::string frameName = (isDelta ? "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07" : 
		 "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/k/%FE%07");
	std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, dataSegments);
	std::vector<boost::shared_ptr<ndn::Data>> parityDataObjects = dataFromParitySegments(frameName, paritySegments);

	dataObjects.insert(dataObjects.end(), parityDataObjects.begin(), parityDataObjects.end());

	std::vector<boost::shared_ptr<ndn::Interest>> interests = getInterests(frameName, 0, dataSegments.size(), 0, paritySegments.size());

	EXPECT_EQ(dataObjects.size(), interests.size());

	std::vector<boost::shared_ptr<WireSegment>> wireSegments;
	for (int i = 0; i < dataObjects.size(); ++i)
	{
		if (i < dataSegments.size())
			wireSegments.push_back(boost::make_shared<WireData<VideoFrameSegmentHeader>>(dataObjects[i], interests[i]));
		else
			wireSegments.push_back(boost::make_shared<WireData<DataSegmentHeader>>(dataObjects[i], interests[i]));

		EXPECT_TRUE(wireSegments.back()->isValid());
	}

	return wireSegments;
}

TEST(TestSampleEstimator, TestTrivialEstimations)
{
	std::srand(std::time(0));
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	SampleEstimator estimator(storage);

	for (int i = 0; i < 30*50; ++i)
	{
		bool isKey = (i%30 == 0);
		unsigned int frameSize = (isKey ? 25000 + std::rand()%5000 : 5000 + std::rand()%1000 );
		std::vector<boost::shared_ptr<WireSegment>> wireSegments = getSegments(frameSize, !isKey);
		for (auto& wireSegment:wireSegments)
			estimator.segmentArrived(wireSegment);
	}

	EXPECT_LE(std::abs((5500./(float)DataSegment<VideoFrameSegmentHeader>::payloadLength(1000)-
		estimator.getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Data))), 1.);
	EXPECT_LE(std::abs((0.2*5500./(float)DataSegment<DataSegmentHeader>::payloadLength(1000)-
		estimator.getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Parity))), 1.);
	EXPECT_LE(std::abs((27500./(float)DataSegment<VideoFrameSegmentHeader>::payloadLength(1000)-
		estimator.getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Data))), 3);

	EXPECT_LE(std::abs((0.2*27500./(float)DataSegment<DataSegmentHeader>::payloadLength(1000)-
		estimator.getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Parity))), 1);

	GT_PRINTF("average delta segnum: data - %.2f; parity - %.2f.\n", 
		estimator.getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Data),
		estimator.getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Parity));
	GT_PRINTF("average key segnum: data - %.2f; parity - %.2f.\n", 
		estimator.getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Data),
		estimator.getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Parity));
	GT_PRINTF("average delta segsize: data - %.2f; parity - %.2f.\n", 
		estimator.getSegmentSizeEstimation(SampleClass::Delta, SegmentClass::Data),
		estimator.getSegmentSizeEstimation(SampleClass::Delta, SegmentClass::Parity));
	GT_PRINTF("average key segsize: data - %.2f; parity - %.2f.\n", 
		estimator.getSegmentSizeEstimation(SampleClass::Key, SegmentClass::Data),
		estimator.getSegmentSizeEstimation(SampleClass::Key, SegmentClass::Parity));
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
