// 
// test-frame-buffer.cc
//
//  Created by Peter Gusev on 28 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <algorithm>
#include <ctime>

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/name.hpp>

#include "gtest/gtest.h"
#include "mock-objects/buffer-observer-mock.hpp"
#include "src/frame-data.hpp"
#include "src/frame-buffer.hpp"
#include "tests-helpers.hpp"
#include "statistics.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace testing;

//******************************************************************************
TEST(TestSlotSegment, TestCreate)
{
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));
	
		SlotSegment seg(interest);
		EXPECT_NE(0, seg.getRequestTimeUsec());
		EXPECT_FALSE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_TRUE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_FALSE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_TRUE(seg.isRightmostRequested());
	}
}

TEST(TestBufferSlot, TestRtx)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";
	std::vector<boost::shared_ptr<const Interest>> interests;
	int n = 10;

	for (int i = 0; i < n; ++i)
	{
		Name iname(frameName);
		iname.appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
	}

	BufferSlot slot;

	EXPECT_EQ(BufferSlot::Free, slot.getState());
	EXPECT_NO_THROW(slot.segmentsRequested(interests));
	EXPECT_EQ(7, slot.getNameInfo().sampleNo_);
	EXPECT_EQ(Name(frameName), slot.getPrefix());

    EXPECT_EQ(0, slot.getRtxNum());
    EXPECT_NO_THROW(slot.segmentsRequested(interests));
    EXPECT_EQ(n, slot.getRtxNum());

    for (auto i:interests)
        EXPECT_EQ(1, slot.getRtxNum(i->getName()));

	EXPECT_EQ(0, slot.getAssembledLevel());
	EXPECT_EQ(BufferSlot::New, slot.getState());
}

TEST(TestBufferSlot, TestAddInterests)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";
	std::vector<boost::shared_ptr<const Interest>> interests;
	int n = 10;

	for (int i = 0; i < n; ++i)
	{
		Name iname(frameName);
		iname.appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
	}

	BufferSlot slot;

	EXPECT_EQ(BufferSlot::Free, slot.getState());
	EXPECT_NO_THROW(slot.segmentsRequested(interests));
	EXPECT_EQ(7, slot.getNameInfo().sampleNo_);
	EXPECT_EQ(Name(frameName), slot.getPrefix());

	EXPECT_EQ(0, slot.getAssembledLevel());
	EXPECT_EQ(BufferSlot::New, slot.getState());
}

TEST(TestBufferSlot, TestAddInterestsTwoTimes)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";
	std::vector<boost::shared_ptr<const Interest>> interests;
	int n = 10;

	for (int i = 0; i < n/2; ++i)
	{
		Name iname(frameName);
		iname.appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
	}

	BufferSlot slot;

	EXPECT_EQ(BufferSlot::Free, slot.getState());
	EXPECT_NO_THROW(slot.segmentsRequested(interests));
	EXPECT_EQ(7, slot.getNameInfo().sampleNo_);
	EXPECT_EQ(Name(frameName), slot.getPrefix());

	EXPECT_EQ(0, slot.getAssembledLevel());
	EXPECT_EQ(BufferSlot::New, slot.getState());

	interests.clear();
	for (int i = n/2; i < n; ++i)
	{
		Name iname(frameName);
		iname.appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
	}

	EXPECT_EQ(BufferSlot::New, slot.getState());
	EXPECT_NO_THROW(slot.segmentsRequested(interests));
	EXPECT_EQ(7, slot.getNameInfo().sampleNo_);
	EXPECT_EQ(Name(frameName), slot.getPrefix());

	EXPECT_EQ(0, slot.getAssembledLevel());
	EXPECT_EQ(BufferSlot::New, slot.getState());

}

TEST(TestBufferSlot, TestBadInterestRange)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";

	{
		std::vector<boost::shared_ptr<const Interest>> interests;
		Name iname(frameName);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
		iname.appendSegment(0);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));

		BufferSlot slot;
		EXPECT_ANY_THROW(slot.segmentsRequested(interests));
		EXPECT_EQ(slot.getState(), BufferSlot::Free);
	}
	{
		std::vector<boost::shared_ptr<const Interest>> interests;
		Name iname(frameName);
		iname.appendSegment(0);
		interests.push_back(boost::make_shared<Interest>(iname, 1000));
		interests.push_back(boost::make_shared<Interest>(Name(frameName), 1000));

		BufferSlot slot;
		EXPECT_ANY_THROW(slot.segmentsRequested(interests));
		EXPECT_NE(BufferSlot::Free, slot.getState());
		slot.clear();
		EXPECT_EQ(BufferSlot::Free, slot.getState());
	}
	{
		std::vector<boost::shared_ptr<const Interest>> interests;
		interests.push_back(boost::make_shared<Interest>(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07").appendSegment(0), 1000));
		interests.push_back(boost::make_shared<Interest>(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%08").appendSegment(0), 1000));

		BufferSlot slot;
		EXPECT_ANY_THROW(slot.segmentsRequested(interests));
		EXPECT_NE(BufferSlot::Free, slot.getState());
		slot.clear();
		EXPECT_EQ(BufferSlot::Free, slot.getState());
	}
}

TEST(TestBufferSlot, TestAddData)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";
	VideoFramePacket vp = getVideoFramePacket();
	std::vector<VideoFrameSegment> segments = sliceFrame(vp);

	{ // all data arrived in order 
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		BufferSlot slot;
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());

		int idx = 0;
		for (auto d:dataObjects)
		{
			boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(d, interests[idx]));
			BufferSlot::State state;

			if (idx == 0) EXPECT_TRUE(slot.getConsistencyState() == BufferSlot::Inconsistent);
			
			slot.segmentReceived(wd);
			// EXPECT_NO_THROW(slot.segmentReceived(wd));
			
			EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::HeaderMeta);
			EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::SegmentMeta);
			EXPECT_EQ(slot.getConsistencyState(), BufferSlot::Consistent);

			if (++idx < dataObjects.size()) 
				EXPECT_EQ(BufferSlot::Assembling, slot.getState());
			else
				EXPECT_EQ(BufferSlot::Ready, slot.getState());

			EXPECT_EQ(round((double)idx/(double)dataObjects.size()*100)/100, 
				round(slot.getAssembledLevel()*100)/100);
			EXPECT_TRUE(slot.hasOriginalSegments());
		}
	}

	{ // all data arrived in random order
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		BufferSlot slot;
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());

		std::random_shuffle(dataObjects.begin(), dataObjects.end());

		int idx = 0;
		for (auto d:dataObjects)
		{
			boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(d, interests[idx]));
			BufferSlot::State state;

			if (idx == 0) EXPECT_TRUE(slot.getConsistencyState() == BufferSlot::Inconsistent);
			
			EXPECT_NO_THROW(slot.segmentReceived(wd));
			
			if (wd->getSampleNo() == 0)
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::HeaderMeta);
			else
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::SegmentMeta);

			if (++idx < dataObjects.size()) 
				EXPECT_EQ(BufferSlot::Assembling, slot.getState());
			else
				EXPECT_EQ(BufferSlot::Ready, slot.getState());

			EXPECT_EQ(round((double)idx/(double)dataObjects.size()*100)/100, 
				round(slot.getAssembledLevel()*100)/100);
			EXPECT_LT(0, slot.getShortestDrd());
			if (idx < dataObjects.size())
			{
				EXPECT_EQ(0, slot.getAssemblingTime());
				EXPECT_EQ(0, slot.getLongestDrd());
			}
		}
		EXPECT_LT(0, slot.getAssemblingTime());
		EXPECT_LT(0, slot.getLongestDrd());
		EXPECT_EQ(slot.getConsistencyState(), BufferSlot::Consistent);
	}

	{ // some data arrived multiple times
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		BufferSlot slot;
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());

		std::random_shuffle(dataObjects.begin(), dataObjects.end());
		std::vector<boost::shared_ptr<ndn::Data>> objects(dataObjects);
		objects.insert(objects.end(), dataObjects.begin(), dataObjects.begin()+3);

		int idx = 0;
		for (auto d:objects)
		{
			boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(d, interests[idx%dataObjects.size()]));
			BufferSlot::State state;

			if (idx == 0) EXPECT_TRUE(slot.getConsistencyState() == BufferSlot::Inconsistent);
			
			EXPECT_NO_THROW(slot.segmentReceived(wd));
			
			if (wd->getSampleNo() == 0)
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::HeaderMeta);
			else
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::SegmentMeta);

			if (++idx < dataObjects.size()) 
				EXPECT_EQ(BufferSlot::Assembling, slot.getState());
			else
				EXPECT_EQ(BufferSlot::Ready, slot.getState());

			if (idx <= dataObjects.size())
				EXPECT_EQ(round((double)idx/(double)dataObjects.size()*100)/100, 
					round(slot.getAssembledLevel()*100)/100);
			else
				EXPECT_EQ(1, slot.getAssembledLevel());
			EXPECT_LT(0, slot.getShortestDrd());
			if (idx < dataObjects.size())
			{
				EXPECT_EQ(0, slot.getAssemblingTime());
				EXPECT_EQ(0, slot.getLongestDrd());
			}
		}
		EXPECT_LT(0, slot.getAssemblingTime());
		EXPECT_LT(0, slot.getLongestDrd());
		EXPECT_EQ(slot.getConsistencyState(), BufferSlot::Consistent);
	}

	{ // add data that was not requested
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		BufferSlot slot;
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());
		boost::shared_ptr<Data> dobj = dataObjects.back();
		dobj->setName(Name(frameName).appendSegment(100));
		boost::shared_ptr<ndn::Interest> i;
		EXPECT_ANY_THROW(slot.segmentReceived(boost::make_shared<WireSegment>(dobj, i)));
	}

	{ // add incorrectly named data
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		BufferSlot slot;
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());
		boost::shared_ptr<Data> dobj = dataObjects.back();
		dobj->setName(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/desktop/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00"));
		boost::shared_ptr<ndn::Interest> i;
		EXPECT_ANY_THROW(slot.segmentReceived(boost::make_shared<WireSegment>(dobj, i)));
	}

	{ // add parity data
		std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
		std::string parityName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00";
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size(), 0, 1);
		BufferSlot slot;

		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());
		boost::shared_ptr<Data> dobj = dataObjects.back();
		EXPECT_NO_THROW(slot.segmentReceived(boost::make_shared<WireSegment>(dobj, interests.back())));

		dobj->setName(Name(parityName));
		EXPECT_NO_THROW(slot.segmentReceived(boost::make_shared<WireSegment>(dobj, interests.back())));
	}
}

TEST(TestBufferSlot, TestMissingSegments)
{
    std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
    VideoFramePacket vp = getVideoFramePacket(30000);
    std::vector<VideoFrameSegment> segments = sliceFrame(vp);
    boost::shared_ptr<ndnrtc::NetworkData> parityData;
    std::vector<ndnrtc::VideoFrameSegment> paritySegments = sliceParity(vp, parityData);
    
    { // all data arrived in random order
        std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
        std::vector<boost::shared_ptr<ndn::Data>> parityObjects = dataFromParitySegments(frameName, paritySegments);
        std::vector<boost::shared_ptr<Interest>> allinterests = getInterests(frameName, 0, dataObjects.size(), 0, parityObjects.size());
        std::vector<boost::shared_ptr<Interest>> dataInterests, parityInterests;
        
        std::copy(allinterests.begin(), allinterests.end()-parityObjects.size(), std::back_inserter(dataInterests));
        std::copy(allinterests.end()-parityObjects.size(), allinterests.end(), std::back_inserter(parityInterests));
        
        BufferSlot slot;
        
        {
            std::vector<boost::shared_ptr<const Interest>> first(dataInterests.begin(), dataInterests.end()-3);
            std::vector<boost::shared_ptr<const Interest>> second(dataInterests.end()-3, dataInterests.end());
            
            slot.segmentsRequested(first);
            boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(dataObjects.front(), dataInterests.front()));
            
            EXPECT_NO_THROW(slot.segmentReceived(wd));
            
            std::vector<ndn::Name> missing = slot.getMissingSegments();
            EXPECT_EQ(missing.size(), second.size());

            for (auto& i:second)
            {
                EXPECT_NE(std::find(missing.begin(), missing.end(), i->getName()), missing.end());
                missing.erase(std::find(missing.begin(), missing.end(), i->getName()));
            }
            slot.segmentsRequested(second);
            
            EXPECT_EQ(slot.getMissingSegments().size(), 0);
        }
        {
            std::vector<boost::shared_ptr<const Interest>> first(parityInterests.begin(), parityInterests.end()-2);
            std::vector<boost::shared_ptr<const Interest>> second(parityInterests.end()-2, parityInterests.end());
            
            slot.segmentsRequested(first);
            boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(parityObjects.front(), parityInterests.front()));
            
            EXPECT_NO_THROW(slot.segmentReceived(wd));
            
            std::vector<ndn::Name> missing = slot.getMissingSegments();
            EXPECT_EQ(missing.size(), second.size());
        
            for (auto& i:second)
            {
                EXPECT_NE(std::find(missing.begin(), missing.end(), i->getName()), missing.end());
                missing.erase(std::find(missing.begin(), missing.end(), i->getName()));
            }
            slot.segmentsRequested(second);
            
            EXPECT_EQ(slot.getMissingSegments().size(), 0);
        }
    }
}

TEST(TestBufferSlot, TestReuseSlot)
{
	BufferSlot slot;

	for (int i = 0; i < 3; ++i)
	{ // all data arrived in random order

		EXPECT_EQ(BufferSlot::Free, slot.getState());
		EXPECT_EQ(SampleClass::Unknown, slot.getNameInfo().class_);
		EXPECT_EQ(0, slot.getAssembledLevel());
		EXPECT_EQ(Name(), slot.getPrefix());
		EXPECT_EQ(BufferSlot::Inconsistent, slot.getConsistencyState());
		EXPECT_EQ(0, slot.getRtxNum());
		EXPECT_EQ(0, slot.getAssemblingTime());
		EXPECT_EQ(0, slot.getShortestDrd());
		EXPECT_EQ(0, slot.getLongestDrd());
		EXPECT_FALSE(slot.hasOriginalSegments());

		std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07";
		VideoFramePacket vp = getVideoFramePacket();
		std::vector<VideoFrameSegment> segments = sliceFrame(vp);
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
		std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
		
		slot.segmentsRequested(makeInterestsConst(interests));
		ASSERT_EQ(BufferSlot::New, slot.getState());

		std::random_shuffle(dataObjects.begin(), dataObjects.end());

		int idx = 0;
		for (auto d:dataObjects)
		{
			boost::shared_ptr<WireSegment> wd(boost::make_shared<WireSegment>(d, interests[idx]));
			BufferSlot::State state;

			if (idx == 0) EXPECT_TRUE(slot.getConsistencyState() == BufferSlot::Inconsistent);
			
			EXPECT_NO_THROW(slot.segmentReceived(wd));
			
			if (wd->getSampleNo() == 0)
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::HeaderMeta);
			else
				EXPECT_TRUE(slot.getConsistencyState()&BufferSlot::SegmentMeta);

			if (++idx < dataObjects.size())
            {
				EXPECT_EQ(BufferSlot::Assembling, slot.getState());
                EXPECT_ANY_THROW(slot.toggleLock());
            }
			else
				EXPECT_EQ(BufferSlot::Ready, slot.getState());

			EXPECT_EQ(round((double)idx/(double)dataObjects.size()*100)/100, 
				round(slot.getAssembledLevel()*100)/100);
			EXPECT_LT(0, slot.getShortestDrd());
			if (idx < dataObjects.size())
			{
				EXPECT_EQ(0, slot.getAssemblingTime());
				EXPECT_EQ(0, slot.getLongestDrd());
			}
		}
		EXPECT_LT(0, slot.getAssemblingTime());
		EXPECT_LT(0, slot.getLongestDrd());
		EXPECT_EQ(slot.getConsistencyState(), BufferSlot::Consistent);
        
        EXPECT_NO_THROW(slot.toggleLock());
        EXPECT_EQ(BufferSlot::Locked, slot.getState());

		slot.clear();
	}
}

TEST(TestVideoFrameSlot, TestAsembleVideoFrame)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
	VideoFramePacket vp = getVideoFramePacket();

	std::vector<VideoFrameSegment> segments = sliceFrame(vp, 734, 1249);

	// all data arrived in random order
	std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
	std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
	BufferSlot slot;
	
	slot.segmentsRequested(makeInterestsConst(interests));
	ASSERT_EQ(BufferSlot::New, slot.getState());

	std::random_shuffle(dataObjects.begin(), dataObjects.end());

	int idx = 0;
	for (auto d:dataObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx]));
		BufferSlot::State state;
		
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		idx++;
	}
	
	VideoFrameSlot videoSlot;
	bool recovered = false;
	boost::shared_ptr<ImmutableVideoFramePacket> videoPacket = videoSlot.readPacket(slot, recovered);

	ASSERT_TRUE(videoPacket.get());
	EXPECT_FALSE(recovered);
	EXPECT_TRUE(checkVideoFrame(videoPacket->getFrame()));
	EXPECT_EQ(734, videoSlot.readSegmentHeader(slot).playbackNo_);
	EXPECT_EQ(1249, videoSlot.readSegmentHeader(slot).pairedSequenceNo_);
}

TEST(TestVideoFrameSlot, TestFailedAssembleNotEnoughData)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
	VideoFramePacket vp = getVideoFramePacket();

	std::vector<VideoFrameSegment> segments = sliceFrame(vp);

	// all data arrived in random order
	std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
	std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
	BufferSlot slot;
	
	slot.segmentsRequested(makeInterestsConst(interests));
	ASSERT_EQ(BufferSlot::New, slot.getState());

	std::random_shuffle(dataObjects.begin(), dataObjects.end());

	int idx = 0;
	for (auto d:dataObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx]));
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		if (idx++ == 3) break;
	}
	
	VideoFrameSlot videoSlot;
	bool recovered = false;
	boost::shared_ptr<ImmutableVideoFramePacket> videoPacket = videoSlot.readPacket(slot, recovered);

	EXPECT_FALSE(recovered);
	EXPECT_FALSE(videoPacket.get());
}

TEST(TestVideoFrameSlot, TestAssembleVideoFrameRecover)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
	VideoFramePacket vp = getVideoFramePacket(10000);

	boost::shared_ptr<NetworkData> parity;
	std::vector<VideoFrameSegment> segments = sliceFrame(vp);
	std::vector<VideoFrameSegment> paritySegments = sliceParity(vp, parity);

	// all data arrived in random order
	std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
	std::vector<boost::shared_ptr<ndn::Data>> parityObjects = dataFromParitySegments(frameName, paritySegments);
	std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size());
	std::vector<boost::shared_ptr<Interest>> parityInterests = getInterests(frameName+"/_parity", 0, parityObjects.size());
	BufferSlot slot;
	
	slot.segmentsRequested(makeInterestsConst(interests));
	slot.segmentsRequested(makeInterestsConst(parityInterests));
	ASSERT_EQ(BufferSlot::New, slot.getState());

	std::random_shuffle(dataObjects.begin(), dataObjects.end());
	std::random_shuffle(parityObjects.begin(), parityObjects.end());

	int idx = 0;
	for (auto d:dataObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx]));
		BufferSlot::State state;
		
		ASSERT_TRUE(wd->isValid());
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		if (++idx == dataObjects.size()-1) break;
	}

	idx = 0;
	for (auto p:parityObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(boost::make_shared<WireData<VideoFrameSegmentHeader>>(p, parityInterests[idx]));
		ASSERT_TRUE(wd->isValid());
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		if (slot.getAssembledLevel() > 1) break;
		idx++;
	}
	
	EXPECT_LT(1, slot.getAssembledLevel());

	VideoFrameSlot videoSlot;
	bool recovered = false;
	boost::shared_ptr<ImmutableVideoFramePacket> videoPacket = videoSlot.readPacket(slot, recovered);

	EXPECT_TRUE(recovered);
	EXPECT_TRUE(videoPacket.get());
}

TEST(TestVideoFrameSlot, TestAssembleVideoFrameRecover2)
{
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07";
	VideoFramePacket vp = getVideoFramePacket(20000);

	boost::shared_ptr<NetworkData> parity;
	std::vector<VideoFrameSegment> segments = sliceFrame(vp);
	std::vector<VideoFrameSegment> paritySegments = sliceParity(vp, parity);

	// all data arrived in random order
	std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
	std::vector<boost::shared_ptr<ndn::Data>> parityObjects = dataFromParitySegments(frameName, paritySegments);
	std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName, 0, dataObjects.size(), 0, parityObjects.size());
	std::vector<boost::shared_ptr<Interest>> parityInterests(interests.end()-parityObjects.size(), interests.end());
	BufferSlot slot;
	
	slot.segmentsRequested(makeInterestsConst(interests));
	slot.segmentsRequested(makeInterestsConst(parityInterests));
	ASSERT_EQ(BufferSlot::New, slot.getState());

	std::random_shuffle(dataObjects.begin(), dataObjects.end());
	std::random_shuffle(parityObjects.begin(), parityObjects.end());

	EXPECT_EQ(0, slot.getAssembledLevel());

	int idx = 0;
	for (auto p:parityObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(
            boost::make_shared<WireData<VideoFrameSegmentHeader>>(p, parityInterests[idx]));
		ASSERT_TRUE(wd->isValid());
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		idx++;
	}

	idx = 0;
	for (auto d:dataObjects)
	{
		boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(
            boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx]));
		BufferSlot::State state;
		
		ASSERT_NO_THROW(slot.segmentReceived(wd));
		if (++idx == dataObjects.size()-parityObjects.size()/2) break;
	}

	EXPECT_LT(1, slot.getAssembledLevel());

	VideoFrameSlot videoSlot;
	bool recovered = false;
	boost::shared_ptr<ImmutableVideoFramePacket> videoPacket = videoSlot.readPacket(slot, recovered);

	EXPECT_TRUE(recovered);
	EXPECT_TRUE(videoPacket.get());
}

TEST(TestAudioBundleSlot, TestAssembleAudioBundle)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    bundlePacket.setHeader(hdr);

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len)/AudioBundlePacket::AudioSampleBlob::wireLength(data_len), 
        bundlePacket.getSamplesNum());

    std::vector<CommonSegment> segments = CommonSegment::slice(bundlePacket, wire_len);
    ASSERT_EQ(1, segments.size());

    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%00";
    ndn::Name n(frameName);
    n.appendSegment(0);
    boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
    ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(0));
    ds->setContent(segments.front().getNetworkData()->data());

	std::vector<boost::shared_ptr<Interest>> interests;
	boost::shared_ptr<Interest> i(boost::make_shared<Interest>(Name(frameName).appendSegment(0), 1000));
	int nonce = 0x1234;
	i->setNonce(ndn::Blob((uint8_t*)&nonce, sizeof(nonce)));
	interests.push_back(i);

	boost::shared_ptr<WireData<DataSegmentHeader>> wd(new WireData<DataSegmentHeader>(ds, interests.front()));

    BufferSlot slot;
    slot.segmentsRequested(makeInterestsConst(interests));
    slot.segmentReceived(wd);

    AudioBundleSlot bundleSlot;
    boost::shared_ptr<ImmutableAudioBundlePacket> p = bundleSlot.readBundle(slot);
    ASSERT_TRUE(p.get());
    EXPECT_EQ(bundlePacket.getSamplesNum(), p->getSamplesNum());

    for (int i = 0; i < p->getSamplesNum(); ++i)
    {
    	bool identical = true;
    	for (int j = 0; j < data_len; ++j)
    		identical &= (rtpData[j] == (*p)[i].data()[j]);
    	EXPECT_TRUE(identical);
    }
}

TEST(TestSlotPool, TestPopPush)
{
	SlotPool pool(10);

	for (int i = 0; i < 10; ++i)
		boost::shared_ptr<BufferSlot> slot = pool.pop();
	EXPECT_FALSE(pool.pop().get());
	EXPECT_EQ(0, pool.size());
	EXPECT_EQ(10, pool.capacity());

	for (int i = 0; i < 10; ++i)
		EXPECT_TRUE(pool.push(boost::make_shared<BufferSlot>()));
	
	EXPECT_FALSE(pool.push(boost::make_shared<BufferSlot>()));
}

TEST(TestBuffer, TestRequestAndReceive)
{
	std::srand(std::time(0));
	size_t poolSize = 50;
	int n = poolSize+10;
	std::string frameName = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d";
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(poolSize));
	MockBufferObserver observer1, observer2;
	Buffer buffer(storage, pool);

	buffer.attach(&observer1);
	buffer.attach(&observer2);

	for (int i = 0; i < n; ++i)
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

		if (i < poolSize)
		{
			EXPECT_CALL(observer1, onNewRequest(_))
				.Times(1);
			EXPECT_CALL(observer2, onNewRequest(_))
				.Times(1);

			EXPECT_TRUE(buffer.requested(interests));
			EXPECT_EQ(i+1, buffer.getSlotsNum(Name(frameName), BufferSlot::New));
		}
		else
		{
			EXPECT_CALL(observer1, onNewRequest(_))
				.Times(0);
			EXPECT_CALL(observer2, onNewRequest(_))
				.Times(0);
			EXPECT_FALSE(buffer.requested(interests));
		}
	}

	for (int i = 0; i < poolSize; ++i)
	{
		VideoFramePacket vp = getVideoFramePacket();
		std::vector<VideoFrameSegment> segments = sliceFrame(vp);
	
		Name n(frameName);
		n.appendSequenceNumber(i);
		std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(n.toUri(), segments);
		
		std::random_shuffle(dataObjects.begin(), dataObjects.end());
	
		int idx = 0;
		for (auto d:dataObjects)
		{
			boost::function<void(const BufferReceipt&)> checkFrame = [](const BufferReceipt& r){
				VideoFrameSlot vs;
				bool recovered = false;
				if (r.slot_->getState() == BufferSlot::Ready)
					EXPECT_TRUE(vs.readPacket(*(r.slot_), recovered).get());
			};

			boost::shared_ptr<ndn::Interest> interest(boost::make_shared<ndn::Interest>(n,1000));
			int nonce = 0;
			interest->setNonce(Blob((uint8_t*)&(nonce), sizeof(int)));

			boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interest));
			EXPECT_CALL(observer1, onNewData(_))
				.Times(1);
			EXPECT_CALL(observer2, onNewData(_))
				.Times(1)
				.WillOnce(Invoke(checkFrame));
			BufferReceipt rcpt = buffer.received(wd);
			idx++;
			if (idx == dataObjects.size())
				EXPECT_EQ(BufferSlot::Ready, rcpt.slot_->getState());
			else
				EXPECT_EQ(1, buffer.getSlotsNum(Name(frameName), BufferSlot::Assembling));
		}

		EXPECT_EQ(i+1, buffer.getSlotsNum(Name(frameName), BufferSlot::Ready));
		EXPECT_EQ(poolSize-i-1, buffer.getSlotsNum(Name(frameName), BufferSlot::New));
	}
    
    EXPECT_EQ(poolSize, (*storage)[Indicator::AssembledNum]);
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
