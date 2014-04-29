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
#include "video-renderer.h"
#import "objc/cocoa-renderer.h"
#include "ndnrtc-testing.h"

using namespace ndnrtc;
using namespace ndnrtc::testing;
using namespace webrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);
::testing::Environment* const env2 = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

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

//********************************************************************************
/**
 * @name VideoRenderer class tests
 */
class VideoRendererTester : public NdnRtcObjectTestHelper
{
public:
    VideoRendererTester():deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    deliverEvent_(*EventWrapper::Create()),
    processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                                kHighPriority)){
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
    VideoRenderer *r_;
    ParamsStruct p_;
    
    webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
    webrtc::ThreadWrapper &processThread_;
    webrtc::EventWrapper &deliverEvent_;
    webrtc::I420VideoFrame deliverFrame_;
    
    static bool processDeliveredFrame(void *obj) {
        return ((VideoRendererTester*)obj)->process();
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

TEST_F(VideoRendererTester, CreateDelete)
{
    VideoRenderer *nr = new VideoRenderer(0,p_);
    delete nr;
}

TEST_F(VideoRendererTester, TestInit)
{
    VideoRenderer *nr = new VideoRenderer(0, p_);
    nr->setLogger(&Logger::sharedInstance());
    nr->setObserver(this);
    flushFlags();
    
    EXPECT_EQ(0, nr->init());
    EXPECT_FALSE(obtainedError_);

    delete nr;
}

TEST_F(VideoRendererTester, TestBadInit)
{
    { // test bad width
        p_.renderWidth = -1;
        
        VideoRenderer *nr = new VideoRenderer(0, p_);
        nr->setLogger(&Logger::sharedInstance());
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
        VideoRenderer *nr = new VideoRenderer(0, p_);
        nr->setLogger(&Logger::sharedInstance());
        nr->setObserver(this);
        flushFlags();
        
        int res = nr->init();
        
        EXPECT_EQ(RESULT_WARN, res);
        EXPECT_TRUE(RESULT_NOT_OK(res));
        EXPECT_TRUE(obtainedError_);
        
        delete nr;
    }
}

TEST_F(VideoRendererTester, TestInitAndStart)
{
    VideoRenderer r(0,p_);
    r.setLogger(&Logger::sharedInstance());
    r.init();
    WAIT(1000);
    
    r.startRendering("Hello");
    WAIT(1000);
    
    r.stopRendering();
}

TEST_F(VideoRendererTester, TestRender)
{
    FrameReader fr("resources/bipbop10.yuv");
    
    VideoRenderer r(1,p_);
    
    r.setLogger(&Logger::sharedInstance());
    r.setObserver(this);
    r.init();
    r.startRendering("sample");
    WAIT(1000);
    
    webrtc::I420VideoFrame frame, nextFrame;
    unsigned int nFrames = 0;
    
    if (fr.readFrame(frame) >= 0)
    {
        while (fr.readFrame(nextFrame) >= 0)
        {
            // use saved render time for calculating rendering delay
            int sleepMs = nextFrame.render_time_ms()-frame.render_time_ms();
            
            nFrames++;
            // set current render timestamp (otherwise renderer will not work)
            frame.set_render_time_ms(TickTime::MillisecondTimestamp());
            
            r.onDeliverFrame(frame);
            frame.SwapFrame(&nextFrame);
            
            usleep(sleepMs*1000);
        }
    }
    
    Logger::sharedInstance().log(NdnLoggerLevelTrace)
    << "total frames number: " << nFrames << endl;
    
    r.stopRendering();
}

class RendererTester : public VideoRenderer
{
public:
    RendererTester(int id, const ParamsStruct &params):
    VideoRenderer(id, params){}
    
    void* getRenderWindow(){
        return renderWindow_;
    }
};

// covers bug #107 http://redmine.named-data.net/issues/1107
TEST_F(VideoRendererTester, TestBlackRender)
{
    for (int i = 0; i < 10; i++)
    {
        RendererTester r(1,p_);
        
        r.init();
        r.startRendering("sample");
        usleep(20000);
        EXPECT_FALSE(getGLView(r.getRenderWindow()) == 0);
        r.stopRendering();
        usleep(10000);
    }
}