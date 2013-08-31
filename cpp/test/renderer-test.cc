//
//  renderer.cc
//  ndnrtc
//
//  Created by Peter Gusev on 8/30/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "renderer.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);

//********************************************************************************
/**
 * @name NdnRendererParams class tests
 */
TEST(VideoRendererParams, CreateDelete)
{
    NdnRendererParams *p = NdnRendererParams::defaultParams();
    delete p;
}
TEST(VideoRendererParams, TestDfaults)
{
    NdnRendererParams *p = NdnRendererParams::defaultParams();
    
    int width, height;
    
    EXPECT_EQ(0, p->getWindowWidth(&width));
    EXPECT_EQ(640, width);
    
    EXPECT_EQ(0, p->getWindowHeight(&height));
    EXPECT_EQ(480, height);
    
    delete p;
}

//********************************************************************************
/**
 * @name NdnRenderer class tests
 */
class NdnRendererTester : public NdnRtcObjectTestHelper
{
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        p = NdnRendererParams::defaultParams();
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        delete p;
    }
    
protected:
    NdnRendererParams *p;
};

TEST_F(NdnRendererTester, CreateDelete)
{
    NdnRenderer *nr = new NdnRenderer(0,p);
    delete nr;
}

TEST_F(NdnRendererTester, TestInit)
{
    NdnRenderer *nr = new NdnRenderer(0,p);
    
    nr->setObserver(this);
    flushFlags();
    
    EXPECT_EQ(0, nr->init());
    EXPECT_FALSE(obtainedError_);
    delete nr;
}


