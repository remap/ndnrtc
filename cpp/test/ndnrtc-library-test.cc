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
    
    EXPECT_NE(NULL, (int64_t)libObject);
    
    if (libObject)
        NdnRtcLibrary::destroyLibraryObject(libObject);
}

class NdnRtcLibraryTester : public INdnRtcLibraryObserver,
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
//    NdnLibParams params ;
}
