//
//  ndnrtc-utils-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 9/24/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;

TEST(NdnRtcUtilsTets, TestSegmentsNumber)
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

