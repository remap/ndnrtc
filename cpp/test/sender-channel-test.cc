 //
//  sender-channel-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "sender-channel.h"

//#undef NDN_LOGGING
using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
::testing::Environment* const env2 = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
/**
 * @name NdnSenderChannel class tests
 */
class NdnSenderChannelTest : public NdnRtcObjectTestHelper
{
    void SetUp(){
        NdnRtcObjectTestHelper::SetUp();
        p_ = DefaultParams;
        audioP_ = DefaultParamsAudio;
    }
    void TearDown(){
        NdnRtcObjectTestHelper::TearDown();
    }
protected:
    ParamsStruct p_, audioP_;
};

TEST_F(NdnSenderChannelTest, CreateDelete)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_, audioP_);
    delete sc;
}

TEST_F(NdnSenderChannelTest, TestInit)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_, audioP_);
    sc->setObserver(this);
    
    int res = sc->init();
    
    EXPECT_EQ(RESULT_OK,res);
    EXPECT_FALSE(obtainedError_);
    
    if (obtainedError_)
        LOG_TRACE("got error %s", obtainedEmsg_);
    
    delete sc;
}

TEST_F(NdnSenderChannelTest, TestTransmission)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_, audioP_);
    
    sc->setObserver(this);
    ASSERT_EQ(RESULT_OK, sc->init());
    
    EXPECT_NO_THROW({
        EXPECT_EQ(RESULT_OK, sc->startTransmission());
        EXPECT_FALSE(obtainedError_);
        WAIT(5000);
        EXPECT_EQ(RESULT_OK, sc->stopTransmission());
        EXPECT_FALSE(obtainedError_);
    });
    
    delete sc;
    
}
