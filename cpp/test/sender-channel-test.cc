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

//********************************************************************************
/**
 * @name NdnSenderChannel class tests
 */
TEST(NdnSenderChannelParams, CreateDeleteParams)
{
    NdnParams *p = NdnSenderChannel::defaultParams();
    delete p;
}
TEST(NdnSenderChannelParams, TestParams)
{
    NdnParams *p = NdnSenderChannel::defaultParams();
    
    {
        // check camera capturer params
        int width, height, fps, deviceId;        
        CameraCapturerParams *ccp = static_cast<CameraCapturerParams*>(p);
        ASSERT_NE(ccp, nullptr);
        
        EXPECT_EQ(0, ccp->getDeviceId(&deviceId));
        EXPECT_EQ(0, ccp->getWidth(&width));
        EXPECT_EQ(0, ccp->getHeight(&height));
        EXPECT_EQ(0, ccp->getFPS(&fps));
    }
 
    { // check renrerer params
        int width, height;
        NdnRendererParams *ccp = static_cast<NdnRendererParams*>(p);
        ASSERT_NE(ccp, nullptr);
        
        EXPECT_EQ(0, ccp->getWindowWidth(&width));
        EXPECT_EQ(0, ccp->getWindowHeight(&height));
    }
    
    {// check video coder params
        int width, height, maxBitRate, startBitRate, frameRate;
        NdnVideoCoderParams *ccp = static_cast<NdnVideoCoderParams*>(p);
        ASSERT_NE(ccp, nullptr);
        
        EXPECT_EQ(0, ccp->getFrameRate(&frameRate));
        EXPECT_EQ(0, ccp->getMaxBitRate(&maxBitRate));
        EXPECT_EQ(0, ccp->getStartBitRate(&startBitRate));
        EXPECT_EQ(0, ccp->getWidth(&width));
        EXPECT_EQ(0, ccp->getHeight(&height));
    }
    
    {// check video sender params
        VideoSenderParams *ccp = static_cast<VideoSenderParams*>(p);
        ASSERT_NE(ccp, nullptr);
    }
    
    delete p;
}

//********************************************************************************
/**
 * @name NdnSenderChannel class tests
 */
class NdnSenderChannelTest : public NdnRtcObjectTestHelper
{
    void SetUp(){
        NdnRtcObjectTestHelper::SetUp();
        p_ = NdnSenderChannel::defaultParams();
    }
    void TearDown(){
        NdnRtcObjectTestHelper::TearDown();
        delete p_;
    }
protected:
    NdnParams *p_;
};

TEST_F(NdnSenderChannelTest, CreateDelete)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_);
    delete sc;
}
TEST_F(NdnSenderChannelTest, TestInit)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_);
    
    EXPECT_EQ(0,sc->init());
    EXPECT_FALSE(obtainedError_);
    
    delete sc;
}
TEST_F(NdnSenderChannelTest, TestTransmission)
{
    NdnSenderChannel *sc = new NdnSenderChannel(p_);
    
    sc->setObserver(this);
    sc->init();
    
    EXPECT_NO_THROW({
        EXPECT_EQ(0, sc->startTransmission());
        EXPECT_FALSE(obtainedError_);
        WAIT(5000);
        EXPECT_EQ(0, sc->stopTransmission());
        EXPECT_FALSE(obtainedError_);
    });
    
    delete sc;
    
}
