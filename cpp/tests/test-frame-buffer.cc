// 
// test-frame-buffer.cc
//
//  Created by Peter Gusev on 28 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/name.hpp>

#include "gtest/gtest.h"
#include "src/frame-buffer.h"

using namespace ndnrtc;
using namespace ndn;

TEST(TestSlotSegment, TestCreate)
{
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));
	
		SlotSegment seg(interest);
		EXPECT_NE(0, seg.getRequestTimeUsec());
		EXPECT_FALSE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_TRUE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/%FE%07/%00%00";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_FALSE(seg.isRightmostRequested());
	}
	{
		std::string name = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd";
		boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(Name(name), 1000));
		EXPECT_NO_THROW(SlotSegment s(interest));

		SlotSegment seg(interest);
		EXPECT_TRUE(seg.isRightmostRequested());
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
