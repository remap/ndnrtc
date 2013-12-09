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

//******************************************************************************
class FrameGrabber : public IRawFrameConsumer
{
public:
    
    FrameGrabber():deliver_cs_(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
    deliverEvent_(*webrtc::EventWrapper::Create()),
    completionEvent_(*webrtc::EventWrapper::Create()),
    processThread_(*webrtc::ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                                        webrtc::kHighPriority))
    {
        cameraParams_ = DefaultParams;
        cameraParams_.captureDeviceId = 1;
        obtainedFramesCount_ = 0;
    }
    
    void onDeliverFrame(webrtc::I420VideoFrame &frame)
    {
        deliver_cs_->Enter();
        obtainedFramesCount_++;
        deliverFrame_.SwapFrame(&frame);
        TRACE("captured frame %d %ld %dx%d", frame.timestamp(),
              frame.render_time_ms(), frame.width(), frame.height());
        deliver_cs_->Leave();
        
        deliverEvent_.Set();
    }
    
    // grabs frames from camera and stores them in a file consequently
    void grabFrames(const char *fileName, unsigned int seconds)
    {
        f_ = fopen(fileName, "w");
        
        cc_ = new CameraCapturer(cameraParams_);
        
        cc_->init();
        cc_->setFrameConsumer(this);
        
        grabbingDuration_ = seconds*1000;
        startTime_ = NdnRtcUtils::millisecondTimestamp();
        
        unsigned int t;
        processThread_.Start(t);
        cc_->startCapture();
        
        completionEvent_.Wait(WEBRTC_EVENT_INFINITE);
        
        cc_->stopCapture();
        deliverEvent_.Set();
        processThread_.SetNotAlive();
        processThread_.Stop();
        
        fclose(f_);
    }
    
protected:
    webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
    webrtc::ThreadWrapper &processThread_;
    webrtc::EventWrapper &deliverEvent_, &completionEvent_;
    webrtc::I420VideoFrame deliverFrame_;
    
    uint64_t startTime_, grabbingDuration_;
    int obtainedFramesCount_ = 0, byteCounter_ = 0;
    ParamsStruct cameraParams_;
    CameraCapturer *cc_;
    FILE *f_;
    
    static bool processDeliveredFrame(void *obj) {
        return ((FrameGrabber*)obj)->process();
    }
    
    bool process()
    {
        uint64_t t = NdnRtcUtils::millisecondTimestamp();
        
        if (t-startTime_ >= grabbingDuration_)
        {
            completionEvent_.Set();
            return false;
        }
        
        if (deliverEvent_.Wait(100) == webrtc::kEventSignaled) {
            
            deliver_cs_->Enter();
            if (!deliverFrame_.IsZeroSize()) {
                /*
                 How frame is stored in file:
                 - render time ms (uint64)
                 - width (int)
                 - height (int)
                 - stride Y (int)
                 - stride U (int)
                 - stride V (int)
                 - Y plane buffer size (int)
                 - Y plane buffer (uint8_t*)
                 - U plane buffer size (int)
                 - U plane buffer (uint8_t*)
                 - V plane buffer size (int)
                 - V plane buffer (uint8_t*)
                 */
                
                uint64_t renderTime = deliverFrame_.render_time_ms();
                int width = deliverFrame_.width();
                int height = deliverFrame_.height();
                int stride_y = deliverFrame_.stride(webrtc::kYPlane);
                int stride_u = deliverFrame_.stride(webrtc::kUPlane);
                int stride_v = deliverFrame_.stride(webrtc::kVPlane);
                int size_y = deliverFrame_.allocated_size(webrtc::kYPlane);
                int size_u = deliverFrame_.allocated_size(webrtc::kUPlane);
                int size_v = deliverFrame_.allocated_size(webrtc::kVPlane);
                
                fwrite(&renderTime, sizeof(uint64_t), 1, f_);
                fwrite(&width, sizeof(int), 1, f_);
                fwrite(&height, sizeof(int), 1, f_);
                
                fwrite(&stride_y, sizeof(int), 1, f_);
                fwrite(&stride_u, sizeof(int), 1, f_);
                fwrite(&stride_v, sizeof(int), 1, f_);
                
                fwrite(&size_y, sizeof(int), 1, f_);
                fwrite(deliverFrame_.buffer(webrtc::kYPlane), sizeof(uint8_t),
                       size_y, f_);
                
                fwrite(&size_u, sizeof(int), 1, f_);
                fwrite(deliverFrame_.buffer(webrtc::kUPlane), sizeof(uint8_t),
                       size_u, f_);
                
                fwrite(&size_v, sizeof(int), 1, f_);
                fwrite(deliverFrame_.buffer(webrtc::kVPlane), sizeof(uint8_t),
                       size_v, f_);
                
                int savedSize = sizeof(uint64_t)+8*sizeof(int)+size_y+size_u+size_v;
                byteCounter_ += savedSize;
                TRACE("saved frame %ld %dx%d (%d bytes. %d total)",
                      deliverFrame_.render_time_ms(),
                      deliverFrame_.width(), deliverFrame_.height(),
                      savedSize, byteCounter_);
            }
            
            deliver_cs_->Leave();
        }
        
        return true;
    }
};

//::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
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
        TRACE("got frame %d %ld %dx%d", obtainedFramesCount_,
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
    TRACE("captured frames - %d", obtainedFramesCount_);
    EXPECT_TRUE(obtainedFrame_);
    
    EXPECT_EQ(cc->stopCapture(),0);
    EXPECT_FALSE(obtainedError_);
    delete cc;
#endif
}
