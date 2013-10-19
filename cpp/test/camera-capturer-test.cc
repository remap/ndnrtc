//
//  camera-capturer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//

#include "camera-capturer.h"
#include "test-common.h"

using namespace ndnrtc;

//::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
/**
 * @name CameraCaptureParams class tests
 */
TEST(CameraCapturerParamsTest, CreateDelete)
{
    CameraCapturerParams *p = CameraCapturerParams::defaultParams();
    delete p;
}
TEST(CameraCapturerParamsTest, CheckDefaults)
{
    CameraCapturerParams *p = CameraCapturerParams::defaultParams();
    
    int width, height, deviceid, fps;
    
    EXPECT_EQ(p->getDeviceId(&deviceid),0);
    EXPECT_EQ(0, deviceid);
    
    EXPECT_EQ(p->getWidth(&width),0);
    EXPECT_EQ(640, width);
    
    EXPECT_EQ(p->getHeight(&height),0);
    EXPECT_EQ(480, height);
    
    EXPECT_EQ(p->getFPS(&fps),0);
    EXPECT_EQ(30, fps);
    
    delete p;
}

//********************************************************************************
/**
 * @name CameraCapturer class tests
 */
class CameraCapturerTest : public IRawFrameConsumer, public NdnRtcObjectTestHelper
{
public:
    void SetUp() {
        NdnRtcObjectTestHelper::SetUp();
        
        cameraParams_ = CameraCapturerParams::defaultParams();
        obtainedFramesCount_ = 0;
    }
    void TearDown(){
        NdnRtcObjectTestHelper::TearDown();
        
        delete cameraParams_;
    }
    
    void onDeliverFrame(webrtc::I420VideoFrame &frame)
    {
        obtainedFrame_ = true;
        obtainedFramesCount_++;
    }
    
protected:
    int obtainedFramesCount_ = 0;
    bool obtainedFrame_ = false;
    CameraCapturerParams *cameraParams_;
};

TEST_F(CameraCapturerTest, CreateDelete)
{
    CameraCapturer *cc = new CameraCapturer(cameraParams_);
    delete cc;
}
TEST_F(CameraCapturerTest, TestInit)
{
    CameraCapturer *cc = new CameraCapturer(cameraParams_);
    
    EXPECT_EQ(cc->init(),0);
    
    delete cc;
}
TEST_F(CameraCapturerTest, TestCapture)
{
    CameraCapturer *cc = new CameraCapturer(cameraParams_);
    
    cc->setObserver(this);
    
//    cc->printCapturingInfo();
    
    EXPECT_EQ(cc->init(),0);
    EXPECT_FALSE(obtainedError_);
    
    cc->setFrameConsumer(this);
    
    EXPECT_EQ(cc->startCapture(), 0);
    EXPECT_FALSE(obtainedError_);
    
    EXPECT_TRUE_WAIT(obtainedFramesCount_ >= 5, 5000.);
    EXPECT_TRUE(obtainedFrame_);
    
    EXPECT_EQ(cc->stopCapture(),0);
    EXPECT_FALSE(obtainedError_);
    delete cc;
    
}