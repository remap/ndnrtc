//
//  ndnrtc-library-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

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
    
    void checkParams(const ParamsStruct &p1, const ParamsStruct &p2)
    {
        EXPECT_STREQ(p1.logFile, p2.logFile);
        EXPECT_EQ(p1.loggingLevel, p2.loggingLevel);
        
        EXPECT_EQ(p1.captureDeviceId, p2.captureDeviceId);
        EXPECT_EQ(p1.captureWidth, p2.captureWidth);
        EXPECT_EQ(p1.captureHeight, p2.captureHeight);
        EXPECT_EQ(p1.captureFramerate, p2.captureFramerate);
        
        EXPECT_EQ(p1.renderWidth, p2.renderWidth);
        EXPECT_EQ(p1.renderHeight, p2.renderHeight);
        
        EXPECT_EQ(p1.codecFrameRate, p2.codecFrameRate);
        EXPECT_EQ(p1.startBitrate, p2.startBitrate);
        EXPECT_EQ(p1.maxBitrate, p2.maxBitrate);
        EXPECT_EQ(p1.encodeWidth, p2.encodeWidth);
        EXPECT_EQ(p1.encodeHeight, p2.encodeHeight);
        
        EXPECT_STREQ(p1.host, p2.host);
        EXPECT_EQ(p1.portNum, p2.portNum);
        
        EXPECT_STREQ(p1.producerId, p2.producerId);
        EXPECT_STREQ(p1.streamName, p2.streamName);
        EXPECT_STREQ(p1.streamThread, p2.streamThread);
        EXPECT_STREQ(p1.ndnHub, p2.ndnHub);
        EXPECT_EQ(p1.segmentSize, p2.segmentSize);
        EXPECT_EQ(p1.freshness, p2.freshness);
        EXPECT_EQ(p1.producerRate, p2.producerRate);
        
        EXPECT_EQ(p1.playbackRate, p2.playbackRate);
        EXPECT_EQ(p1.interestTimeout, p2.interestTimeout);
        EXPECT_EQ(p1.bufferSize, p2.bufferSize);
        EXPECT_EQ(p1.slotSize, p2.slotSize);
    }
    void checkAParams(const ParamsStruct &p1, const ParamsStruct &p2)
    {
        EXPECT_STREQ(p1.host, p2.host);
        EXPECT_EQ(p1.portNum, p2.portNum);
        
        EXPECT_STREQ(p1.producerId, p2.producerId);
        EXPECT_STREQ(p1.streamName, p2.streamName);
        EXPECT_STREQ(p1.streamThread, p2.streamThread);
        EXPECT_STREQ(p1.ndnHub, p2.ndnHub);
        EXPECT_EQ(p1.segmentSize, p2.segmentSize);
        EXPECT_EQ(p1.freshness, p2.freshness);
        EXPECT_EQ(p1.producerRate, p2.producerRate);
        
        EXPECT_EQ(p1.playbackRate, p2.playbackRate);
        EXPECT_EQ(p1.interestTimeout, p2.interestTimeout);
        EXPECT_EQ(p1.bufferSize, p2.bufferSize);
        EXPECT_EQ(p1.slotSize, p2.slotSize);
    }
    
};

TEST_F(NdnRtcLibraryTester, TestConfigure)
{
    {// test default params
        ParamsStruct p = DefaultParams;
        ParamsStruct ap = DefaultParamsAudio;
        
        library_->configure(p, ap);
        EXPECT_STREQ("init", receivedState_.c_str());
        
        ParamsStruct resP, resAudioP;
        
        memset(&resP, 0, sizeof(resP));
        memset(&resAudioP, 0, sizeof(resAudioP));
        
        library_->currentParams(resP, resAudioP);
        
        checkParams(p, resP);
        checkAParams(ap, resAudioP);
    }
#if 0
    {// check for default params
        ParamsStruct resP, resAudioP;
        
        memset(&resP, 0, sizeof(resP));
        memset(&resAudioP, 0, sizeof(resAudioP));
        
        library_->currentParams(resP, resAudioP);
        
        checkParams(DefaultParams, resP);
        checkAParams(DefaultParamsAudio, resAudioP);
    }
    {// check custom params
        ParamsStruct p = {
            NdnLoggerDetailLevelNone,    // log level
            "file.log",                   // log file
            
            0,      // capture device id
            320,    // capture width
            240,    // capture height
            15,     // capture framerate
            
            100,    // render width
            100,    // render height
            
            15,     // codec framerate
            60,     // gop
            400,    // codec start bitrate
            2000,   // codec max bitrate
            320,    // codec encoding width
            240,    // codec encoding height
            0,      // drop frames
            
            "borges.metwi.ucla.edu",    // network ndnd remote host
            9695,           // default ndnd port number
            
            "user1",     // producer id
            "video",       // stream name
            "h264",          // stream thread name
            "ndn/apps",     // ndn hub
            1500,   // segment size for media frame
            5,      // data freshness (seconds) value
            15,     // producer rate (currently equal to playback rate)
            
            15,     // playback rate of local consumer
            3,      // interest timeout
            30,     // incoming framebuffer size
            15000  // frame buffer slot size
        };
        
        ParamsStruct ap = {
            NdnLoggerDetailLevelAll,    // log level
            "ndnrtc.log",                   // log file
            
            0,      // capture device id
            0,    // capture width
            0,    // capture height
            0,     // capture framerate
            
            0,    // render width
            0,    // render height
            
            0,     // codec framerate
            0,      // gop
            0,    // codec start bitrate
            0,   // codec max bitrate
            0,    // codec encoding width
            0,    // codec encoding height
            0,      // drop frames
            
            "ndn",    // network ndnd remote host
            0,           // default ndnd port number
            
            "user",     // producer id
            "audio",       // stream name
            "mp3",          // stream thread name
            "ndn/apps",     // ndn hub
            1000,   // segment size for media frame
            4,      // data freshness (seconds) value
            20,     // producer rate (currently equal to playback rate)
            
            20,     // playback rate of local consumer
            2,      // interest timeout
            20,     // incoming framebuffer size
            1500  // frame buffer slot size
        };

        library_->configure(p, ap);
        EXPECT_STREQ("init", receivedState_.c_str());
        
        ParamsStruct resP, resAudioP;
        
        memset(&resP, 0, sizeof(resP));
        memset(&resAudioP, 0, sizeof(resAudioP));
        
        library_->currentParams(resP, resAudioP);
        
        checkParams(p, resP);
        checkAParams(ap, resAudioP);
    }
    {// check recoverable params
        ParamsStruct p = {
            NdnLoggerDetailLevelNone,    // log level
            "file.log",                   // log file
            
            0,      // capture device id
            489289,    // capture width
            240,    // capture height
            0xffff,     // capture framerate
            
            0,    // render width
            100,    // render height
            
            15,     // codec framerate
            30,      // gop
            400,    // codec start bitrate
            2000,   // codec max bitrate
            320,    // codec encoding width
            240,    // codec encoding height
            0,      // drop frames
            
            "ndn",    // network ndnd remote host
            9695,           // default ndnd port number
            
            "user1",     // producer id
            "video",       // stream name
            "h264",          // stream thread name
            "ndn/apps",     // ndn hub
            1500,   // segment size for media frame
            5,      // data freshness (seconds) value
            15,     // producer rate (currently equal to playback rate)
            
            0xffff,     // playback rate of local consumer
            3,      // interest timeout
            30,     // incoming framebuffer size
            0  // frame buffer slot size
        };
        
        ParamsStruct ap = {
            NdnLoggerDetailLevelAll,    // log level
            "ndnrtc.log",                   // log file
            
            0,      // capture device id
            0,    // capture width
            0,    // capture height
            0,     // capture framerate
            
            0,    // render width
            0,    // render height
            
            0,     // codec framerate
            0,      // gop
            0,    // codec start bitrate
            0,   // codec max bitrate
            0,    // codec encoding width
            0,    // codec encoding height
            0,      // drop rames
            
            "ndn",    // network ndnd remote host
            0,           // default ndnd port number
            
            "user",     // producer id
            "audio",       // stream name
            "mp3",          // stream thread name
            "ndn/apps",     // ndn hub
            1,   // segment size for media frame
            4,      // data freshness (seconds) value
            20,     // producer rate (currently equal to playback rate)
            
            0,     // playback rate of local consumer
            2,      // interest timeout
            20,     // incoming framebuffer size
            1  // frame buffer slot size
        };
        
        library_->configure(p, ap);
        EXPECT_STREQ("warn", receivedState_.c_str());
    }
    {// check bad params
        ParamsStruct p = {
            NdnLoggerDetailLevelNone,    // log level
            "file.log",                   // log file
            
            0,      // capture device id
            489289,    // capture width
            240,    // capture height
            0xffff,     // capture framerate
            
            0,    // render width
            100,    // render height
            
            15,     // codec framerate
            0,      // gop
            400,    // codec start bitrate
            2000,   // codec max bitrate
            320,    // codec encoding width
            240,    // codec encoding height
            0,      // drop frames
            
            "",    // network ndnd remote host
            9695,           // default ndnd port number
            
            "user1",     // producer id
            "video",       // stream name
            "h264",          // stream thread name
            "ndn/apps",     // ndn hub
            1500,   // segment size for media frame
            5,      // data freshness (seconds) value
            15,     // producer rate (currently equal to playback rate)
            
            0xffff,     // playback rate of local consumer
            3,      // interest timeout
            30,     // incoming framebuffer size
            15000  // frame buffer slot size
        };
        
        ParamsStruct ap = {
            NdnLoggerDetailLevelAll,    // log level
            "ndnrtc.log",                   // log file
            
            0,      // capture device id
            0,    // capture width
            0,    // capture height
            0,     // capture framerate
            
            0,    // render width
            0,    // render height
            
            0,     // codec framerate
            0,      // gop
            0,    // codec start bitrate
            0,   // codec max bitrate
            0,    // codec encoding width
            0,    // codec encoding height
            0,      // drop frames
            
            "",    // network ndnd remote host
            0,           // default ndnd port number
            
            "user",     // producer id
            "audio",       // stream name
            "mp3",          // stream thread name
            "ndn/apps",     // ndn hub
            1000,   // segment size for media frame
            4,      // data freshness (seconds) value
            20,     // producer rate (currently equal to playback rate)
            
            20,     // playback rate of local consumer
            2,      // interest timeout
            20,     // incoming framebuffer size
            1500  // frame buffer slot size
        };
        
        library_->configure(p, ap);
        EXPECT_STREQ("error", receivedState_.c_str());
    }
#endif
}

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
    {
        ASSERT_NO_THROW(
                        library_->startPublishing("testuser");
                        WAIT(100);
                        library_->stopPublishing();
                        );
    }
    {
        ASSERT_NO_THROW(
                        library_->startPublishing("testuser");
                        WAIT(100);
                        library_->stopPublishing();
                        WAIT(100);                        
                        library_->startPublishing("testuser");
                        WAIT(100);
                        library_->stopPublishing();                        
                        );
    }
}

TEST_F(NdnRtcLibraryTester, TestEmptyUsername)
{
    EXPECT_EQ(RESULT_ERR, library_->startPublishing(""));
    EXPECT_STREQ("error", receivedState_.c_str());
    
    receivedState_ = "";
    
    EXPECT_EQ(RESULT_ERR, library_->startFetching(""));
    EXPECT_STREQ("error", receivedState_.c_str());
}
