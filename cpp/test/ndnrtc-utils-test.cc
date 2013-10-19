//
//  ndnrtc-utils-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(NdnRtcUtilsTests, TestSegmentsNumber)
{
    {
        int segmentSize = 3800;
        int dataSize = 2000;
        int expectedSegmentsNum = 1;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 3800;
        int dataSize = 0;
        int expectedSegmentsNum = 0;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 100;
        int expectedSegmentsNum = 5;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 101;
        int expectedSegmentsNum = 6;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 20;
        int dataSize = 99;
        int expectedSegmentsNum = 5;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 100;
        int dataSize = 100;
        int expectedSegmentsNum = 1;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
    {
        int segmentSize = 100;
        int dataSize = 101;
        int expectedSegmentsNum = 2;
        
        EXPECT_EQ(expectedSegmentsNum, NdnRtcUtils::getSegmentsNumber(segmentSize, dataSize));
    }
}

TEST(NdnRtcUtilsTests, TestFrameNumber)
{
    {
        char *prefixStr = "/ndn/ndnrtc/user/testuser/streams/video0/vp8-640/frames/1";
        Name prefix(prefixStr);
        unsigned int frameNo = NdnRtcUtils::frameNumber(prefix.getComponent(prefix.getComponentCount()-1));
        
        EXPECT_EQ(1, frameNo);
    }
}

