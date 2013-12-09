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
#import "objc/cocoa-renderer.h"

using namespace ndnrtc;
using namespace webrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
::testing::Environment* const env2 = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));
#if 0
TEST(CocoaRenderWindow, TestCreateDestroy)
{
    void *win = createCocoaRenderWindow("NoName", 640, 480);
    
    cout << "you should be able to see black window on your screen" << endl;

    WAIT(2000);
    destroyCocoaRenderWindow(win);
}

TEST(CocoaRenderWindow, TestDestroyTwice)
{
    void *win = createCocoaRenderWindow("NoName", 640, 480);
    
    WAIT(200);
    destroyCocoaRenderWindow(win);
    destroyCocoaRenderWindow(win);
}

TEST(CocoaRenderWindow, TestChangeTitle)
{
    void *win = createCocoaRenderWindow("NoName", 640, 480);
    
    cout << "you should be able to see black window with changing titles" << endl;
    
    WAIT(500);
    setWindowTitle("Title 1", win);
    WAIT(500);
    setWindowTitle("Title 2", win);
    WAIT(500);
    setWindowTitle("Title 3", win);
    WAIT(500);
    setWindowTitle("Title 4", win);
    WAIT(500);
    setWindowTitle("Title 5", win);

    destroyCocoaRenderWindow(win);
}

TEST(CocoaRenderWindow, TestCreateSeveral)
{
    int n = 10;
    void *wins[n];
    
    cout << "creating 10 window..." << endl;
    
    for (int i = 0; i < n; i++)
    {
        wins[i] = createCocoaRenderWindow("Window", 640, 480);
        WAIT(200);
    }

    WAIT(1000);
    
    for (int i = 0; i < n; i++)
    {
        destroyCocoaRenderWindow(wins[i]);
        WAIT(200);
    }
}
#endif
//********************************************************************************
/**
 * @name NdnRenderer class tests
 */
class NdnRendererTester : public NdnRtcObjectTestHelper
{
public:
    NdnRendererTester():deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    deliverEvent_(*EventWrapper::Create()),
    processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                                kHighPriority)){
        TRACE("CREATE");
    }
    
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
    unsigned int shutdown_, curFrame_;
    NdnRenderer *r_;
    ParamsStruct p_;
    
    webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
    webrtc::ThreadWrapper &processThread_;
    webrtc::EventWrapper &deliverEvent_;
    webrtc::I420VideoFrame deliverFrame_;
    
    static bool processDeliveredFrame(void *obj) {
        return ((NdnRendererTester*)obj)->process();
    }
    
    bool process()
    {
        if (deliverEvent_.Wait(100) == kEventSignaled)
        {
            deliver_cs_->Enter();
            if (!deliverFrame_.IsZeroSize()) {
                r_->onDeliverFrame(deliverFrame_);
                
                if (curFrame_ >= shutdown_ && curFrame_ <= shutdown_+10)
                {
                    EXPECT_TRUE(obtainedError_);
                }
            }
            deliver_cs_->Leave();
        }
        
        return true;
    }
};
#if 0
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

TEST_F(NdnRendererTester, TestInitAndStart)
{
    NdnRenderer r(0,p_);
    
    r.init();
    WAIT(1000);
    
    r.startRendering("Hello");
    WAIT(1000);
    
    r.stopRendering();
}
#endif
TEST_F(NdnRendererTester, TestStartStopStart)
{
    FrameReader fr("resources/sample_futurama.yuv");
    r_ = new NdnRenderer(1,p_);
    
    r_->setObserver(this);
    
    r_->init();
    WAIT(1000);
    
    r_->startRendering("sample");
    WAIT(1000);
    
    unsigned int t;
    processThread_.Start(t);
    
    webrtc::I420VideoFrame frame;
    curFrame_ = 0;
    shutdown_ = 100;
    
    unsigned int nTotal = 200;
    
    while (fr.readFrame(frame))
    {
        deliver_cs_->Enter();
        curFrame_++;
        
        if (curFrame_ == shutdown_)
            r_->stopRendering();

        if (curFrame_ == shutdown_+10)
            r_->startRendering("sample. restart");
        
        deliverFrame_.SwapFrame(&frame);
        deliver_cs_->Leave();
        
        deliverEvent_.Set();
        
        usleep(30000);
        
        if (curFrame_ == nTotal)
            break;
    }
    
    r_->stopRendering();
    delete r_;
}
#if 0
TEST_F(NdnRendererTester, TestRender)
{
    FrameReader fr("resources/sample_futurama.yuv");
//    FrameReader fr("resources/sample.yuv");
    
    NdnRenderer r(1,p_);
    
    r.setObserver(this);
    r.init();
    WAIT(1000);
    r.startRendering("sample");
    WAIT(1000);
    
    webrtc::I420VideoFrame frame;
    unsigned int nFrames = 0;
    
    while (fr.readFrame(frame))
    {
        nFrames++;
        r.onDeliverFrame(frame);
        usleep(30000);
    }
    
    TRACE("total frames number: %d", nFrames);
    
    r.stopRendering();
    processThread_.SetNotAlive();
    deliverEvent_.Set();
    processThread.Stop();
}
#endif