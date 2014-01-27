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

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
/**
 * @name CameraCaptureParams class tests
 */
TEST(CameraCapturerParamsTest, CheckDefaults)
{
    ParamsStruct p = DefaultParams;
    
    EXPECT_EQ(0, p.captureDeviceId);
    EXPECT_EQ(640, p.captureWidth);
    EXPECT_EQ(480, p.captureHeight);
    EXPECT_EQ(30, p.captureFramerate);
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
        
        cameraParams_ = DefaultParams;
        cameraParams_.captureDeviceId = 0;
        obtainedFramesCount_ = 0;
    }
    void TearDown(){
        NdnRtcObjectTestHelper::TearDown();
    }
    
    void onDeliverFrame(webrtc::I420VideoFrame &frame)
    {
        obtainedFrame_ = true;
        obtainedFramesCount_++;
        LOG_TRACE("got frame %d %ld %dx%d", obtainedFramesCount_,
              frame.render_time_ms(), frame.width(), frame.height());
    }
    
protected:
    int obtainedFramesCount_ = 0;
    bool obtainedFrame_ = false;
    ParamsStruct cameraParams_;
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
#if 0
    FrameGrabber f;
    f.grabFrames("resources/sample.yuv",10);
#else
    CameraCapturer *cc = new CameraCapturer(cameraParams_);
    
    cc->setObserver(this);
    
    EXPECT_EQ(cc->init(),0);
    EXPECT_FALSE(obtainedError_);
    
    cc->setFrameConsumer(this);
    
    EXPECT_EQ(cc->startCapture(), 0);
    EXPECT_FALSE(obtainedError_);
    
    unsigned int sec = 3;
    
    EXPECT_TRUE_WAIT(obtainedFramesCount_ >= sec*cameraParams_.captureFramerate, 5000.);
    LOG_TRACE("captured frames - %d", obtainedFramesCount_);
    EXPECT_TRUE(obtainedFrame_);
    
    EXPECT_EQ(cc->stopCapture(),0);
    EXPECT_FALSE(obtainedError_);
    delete cc;
#endif
}
