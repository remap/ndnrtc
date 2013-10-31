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

TEST_F(NdnRendererTester, TestInitAndStart)
{
    NdnRenderer r(0,p_);
    
    r.init();
    WAIT(1000);
    
    r.startRendering("Hello");
    WAIT(1000);
    
    r.stopRendering();
}
