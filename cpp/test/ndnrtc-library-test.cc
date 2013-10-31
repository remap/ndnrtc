//
//  ndnrtc-library-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_TRACE

#include "test-common.h"
#include "ndnrtc-library.h"

#define LIB_FNAME "bin/libndnrtc-sa.dylib"

using namespace ndnrtc;
using namespace std;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(NdnRtcLibraryTest, CreateDelete)
{
    NdnRtcLibrary *libObject = NdnRtcLibrary::instantiateLibraryObject(LIB_FNAME);
    
    EXPECT_FALSE(NULL == libObject);
    
    if (libObject)
        NdnRtcLibrary::destroyLibraryObject(libObject);
}

class NdnRtcLibraryTester : public INdnRtcLibraryObserver,
public UnitTestHelperNdnNetwork,
public ::testing::Test
{
public:
    void onStateChanged(const char *state, const char *args)
    {
        allStates_.push_back(string(state));
        allArgs_.push_back(string(args));
        
        receivedState_ = string(state);
        receivedArgs_ = string(args);
    }
    
    void SetUp()
    {
        allStates_.clear();
        allArgs_.clear();
        
        receivedState_ = "";
        receivedArgs_ = "";
        
        library_ = NdnRtcLibrary::instantiateLibraryObject(LIB_FNAME);
        library_->setObserver(this);
        
        ASSERT_NE(nullptr, library_);
    }
    
    void TearDown()
    {
        if (library_)
            NdnRtcLibrary::destroyLibraryObject(library_);
    }
    
protected:
    NdnRtcLibrary *library_ = nullptr;
    
    string receivedState_, receivedArgs_;
    vector<string> allStates_, allArgs_;
};

TEST_F(NdnRtcLibraryTester, TestConfigure)
{
}
#if 0
TEST_F(NdnRtcLibraryTester, TestStartPublishingTwice)
{
    ASSERT_NO_THROW(
                    library_->startPublishing("testuser");
                    WAIT(3000);
                    library_->startPublishing("testuser");
                    WAIT(3000);
                    library_->startPublishing("testuser2");
                    WAIT(3000);
                    library_->stopPublishing();
    );
}

TEST_F(NdnRtcLibraryTester, TestStartStop)
{
    ASSERT_NO_THROW(
                    library_->startPublishing("testuser");
                    library_->stopPublishing();
                    );
}
#endif
TEST_F(NdnRtcLibraryTester, TestEmptyUsername)
{
    EXPECT_EQ(RESULT_ERR, library_->startPublishing(""));
    EXPECT_STREQ("error", receivedState_.c_str());
    
    receivedState_ = "";
    
    EXPECT_EQ(RESULT_ERR, library_->joinConference(""));
    EXPECT_STREQ("error", receivedState_.c_str());
}