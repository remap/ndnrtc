//
//  renderer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "renderer.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
::testing::Environment* const env2 = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
/**
 * @name NdnRenderer class tests
 */
class NdnRendererTester : public NdnRtcObjectTestHelper
{
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        p_ = DefaultParams;
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
    }
    
protected:
    ParamsStruct p_;
};

TEST_F(NdnRendererTester, CreateDelete)
{
    NdnRenderer *nr = new NdnRenderer(0,p_);
    delete nr;
}

TEST_F(NdnRendererTester, TestInit)
{
    NdnRenderer *nr = new NdnRenderer(0, p_);
    
    nr->setObserver(this);
    flushFlags();
    
    EXPECT_EQ(0, nr->init());
    EXPECT_FALSE(obtainedError_);

    delete nr;
}

TEST_F(NdnRendererTester, TestBadInit)
{
    { // test bad width
        p_.renderWidth = -1;
        
        NdnRenderer *nr = new NdnRenderer(0, p_);
        
        nr->setObserver(this);
        flushFlags();
        
        int res = nr->init();
        
        EXPECT_EQ(RESULT_WARN, res);
        EXPECT_TRUE(RESULT_NOT_OK(res));
        EXPECT_TRUE(obtainedError_);
        
        delete nr;
    }
    { // test bad height
        p_.renderHeight = -1;
        NdnRenderer *nr = new NdnRenderer(0, p_);
        
        nr->setObserver(this);
        flushFlags();
        
        int res = nr->init();
        
        EXPECT_EQ(RESULT_WARN, res);
        EXPECT_TRUE(RESULT_NOT_OK(res));
        EXPECT_TRUE(obtainedError_);
        
        delete nr;
    }
}
