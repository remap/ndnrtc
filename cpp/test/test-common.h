//
//  test-common.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/29/13
//

#ifndef ndnrtc_test_common_h
#define ndnrtc_test_common_h

#define ENV_NAME std::string(__FILE__)

#include <unistd.h>
#include <stdint.h>

#include "gtest/gtest.h"
#include "simple-log.h"
#include "webrtc.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "ndnrtc-object.h"

int64_t millisecondTimestamp();

#define WAIT_(ex, timeout, res) \
do { \
res = (ex); \
int64_t start = millisecondTimestamp(); \
while (!res && millisecondTimestamp() < start + timeout) { \
usleep(1000); \
res = (ex); \
} \
} while (0);\

#define WAIT(timeout) \
do { \
bool b = false; \
WAIT_(false, timeout, b); \
} while (0); \

#define EXPECT_TRUE_WAIT(ex, timeout) \
do { \
bool res; \
WAIT_(ex, timeout, res); \
if (!res) EXPECT_TRUE(ex); \
} while (0);

//******************************************************************************
class NdnRtcObjectTestHelper : public ::testing::Test, public ndnrtc::INdnRtcObjectObserver
{
public:
    virtual void SetUp()
    {
        flushFlags();

        const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
        INFO("***");
        INFO("***[GTESTS]: entering test %s:%s", test_info->test_case_name(),test_info->name());
        
#ifdef WEBRTC_LOGGING
        setupWebRTCLogging();
#endif
    }
    virtual void TearDown()
    {
        const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
        INFO("***[GTESTS]: leaving test %s:%s", test_info->test_case_name(),test_info->name());
        INFO("***");
    }

    virtual void onErrorOccurred(const char *errorMessage)
    {
        obtainedError_ = true;
        obtainedEmsg_ = (char*)errorMessage;
    }
    
    static webrtc::EncodedImage *loadEncodedFrame()
    {
        int width = 640, height = 480;
        // change frame size according to the data in resource file file
        
        FILE *f = fopen("resources/vp8_640x480.frame", "rb");

        if (!f)
        {
            return NULL;
        }
        
        int32_t size, length;
        if (!fread(&size, sizeof(size), 1, f))
        {
            return NULL;
        }
        
        if (!fread(&length, sizeof(length), 1, f))
        {
            return NULL;
        }
        
        // don't delete frameData = it is used (but not owned!) by the
        // frame when it is created
        unsigned char* frameData = new unsigned char[size];
        
        if (!fread(frameData, 1, length, f))
        {
            return NULL;
        }
        
        webrtc::EncodedImage *sampleFrame;
        sampleFrame = new webrtc::EncodedImage(frameData, length, size);
        sampleFrame->_encodedWidth = width;
        sampleFrame->_encodedHeight = height;
        sampleFrame->_timeStamp = millisecondTimestamp();
        sampleFrame->capture_time_ms_ = millisecondTimestamp();
        
        fclose(f);
        
        return sampleFrame;
    }
    
    static void checkFrames(webrtc::EncodedImage *f1, webrtc::EncodedImage *f2)
    {
        EXPECT_EQ(f1->_size, f2->_size);
        EXPECT_EQ(f1->_length, f2->_length);
        EXPECT_EQ(f1->_encodedWidth, f2->_encodedWidth);
        EXPECT_EQ(f1->_encodedHeight, f2->_encodedHeight);
        EXPECT_EQ(f1->_timeStamp, f2->_timeStamp);
        EXPECT_EQ(f1->capture_time_ms_, f2->capture_time_ms_);
        EXPECT_EQ(f1->_frameType, f2->_frameType);
        EXPECT_EQ(f1->_completeFrame, f2->_completeFrame);
        
        for (unsigned int i = 0; i < f1->_length; i++)
            EXPECT_EQ(f1->_buffer[i], f2->_buffer[i]);
    }
    
    static int randomInt(int min, int max)
    {
        return rand() % (max-min)+min;
    }
    
protected:
    char *obtainedEmsg_ = NULL;
    bool obtainedError_ = false;

    virtual void flushFlags()
    {
        obtainedEmsg_ = NULL;
        obtainedError_ = false;
    }

    void setupWebRTCLogging(){
        webrtc::Trace::CreateTrace();
        webrtc::Trace::SetTraceFile("bin/webrtc.log");
    }
};

//******************************************************************************
class NdnRtcTestEnvironment : public ::testing::Environment
{
public:
    NdnRtcTestEnvironment(const std::string &name,
                          NdnLoggerDetailLevel logLevel = NdnLoggerDetailLevelAll):Environment()
    {
        name_ = name;
        name_ += ".log";
        logLevel_ = logLevel;
    }
    void SetUp()
    {
        NdnLogger::initialize(name_.c_str(), logLevel_);
        INFO("test suite started. log is here: %s", name_.c_str());
    }
    void TearDown()
    {
        INFO("test suite finished");
    }
    
    void setLogLevel(NdnLoggerDetailLevel lvl)
    {
        logLevel_ = lvl;
    }
    
protected:
    std::string name_;
    NdnLoggerDetailLevel logLevel_;
};


//******************************************************************************
class CocoaTestEnvironment : public ::testing::Environment
{
public:
    void SetUp();
    void TearDown();
    
protected:
    void *pool_;
};

//******************************************************************************
class UnitTestHelperNdnNetwork
{
public:
    virtual void NdnSetUp(string &streamAccessPrefix, string &userPrefix);
    virtual void NdnTearDown();
    
    virtual void onInterest(const shared_ptr<const Name>& prefix,
                            const shared_ptr<const Interest>& interest,
                            ndn::Transport& transport);
    virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);
    
    virtual void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data);
    virtual void onTimeout(const shared_ptr<const Interest>& interest);
    
protected:
    bool isFetching_;
    webrtc::ThreadWrapper *fetchingThread_;
    unsigned int nReceivedInterests_, nReceivedData_, nReceivedTimeout_;
    
    ndnrtc::ParamsStruct params_;
    shared_ptr<ndn::Transport> ndnTransport_, ndnReceiverTransport_;
    shared_ptr<Face> ndnFace_, ndnReceiverFace_;
    shared_ptr<KeyChain> ndnKeyChain_;
    shared_ptr<Name> certName_;

    // publishes audio or video data packet under the specified prefix by
    // splitting data into a segments and appending
    //  <frame_number>/<segment_number> to the prefix
    void publishMediaPacket(unsigned int dataLen, unsigned char *dataPacket,
                            unsigned int frameNo, unsigned int segmentSize,
                            const string &framePrefix, int freshness,
                            bool mixedSendOrder = false);
    
    // publishes data under the prefix
    void publishData(unsigned int dataLen, unsigned char *dataPacket,
                     const string &prefix, int freshness,
                     const Blob& finalBlockId);
    
    void startProcessingNdn();
    void stopProcessingNdn();
    
    static bool fetchThreadFunc(void *obj){
        return ((UnitTestHelperNdnNetwork*)obj)->fetchData();
    }
    bool fetchData();
};

//******************************************************************************
class FrameReader
{
public:
    FrameReader(const char *fileName)
    {
        f_ = fopen(fileName, "r");
    }
    ~FrameReader()
    {
        fclose(f_);
    }
    
    int readFrame(webrtc::I420VideoFrame &frame)
    {
        int readSize = 0;
        
        if (f_)
        {
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
            uint64_t renderTime;
            int width, height,
            stride_y, stride_u, stride_v,
            size_y, size_u, size_v;
            
            readSize = fread(&renderTime, sizeof(uint64_t), 1, f_);
            
            if (readSize != 1)
                return 0;
            
            fread(&width, sizeof(int), 1, f_);
            fread(&height, sizeof(int), 1, f_);
            
            fread(&stride_y, sizeof(int), 1, f_);
            fread(&stride_u, sizeof(int), 1, f_);
            fread(&stride_v, sizeof(int), 1, f_);
            
            fread(&size_y, sizeof(int), 1, f_);
            
            uint8_t *buffer_y = (uint8_t*)malloc(size_y);
            fread(buffer_y, sizeof(uint8_t), size_y, f_);
            
            fread(&size_u, sizeof(int), 1, f_);
            
            uint8_t *buffer_u = (uint8_t*)malloc(size_u);
            fread(buffer_u, sizeof(uint8_t), size_u, f_);
            
            fread(&size_v, sizeof(int), 1, f_);
            
            uint8_t *buffer_v = (uint8_t*)malloc(size_v);
            fread(buffer_v, sizeof(uint8_t), size_v, f_);
            
            int readSize = sizeof(uint64_t)+8*sizeof(int)+size_y+size_u+size_v;
            TRACE("read frame %ld %dx%d (%d bytes)",
                  renderTime,
                  width, height,
                  readSize);
            
            if (-1 == frame.CreateFrame(size_y, buffer_y,
                                        size_u, buffer_u,
                                        size_v, buffer_v,
                                        width, height,
                                        stride_y, stride_u, stride_v))
                NDNERROR("couldn't create frame");
            
            frame.set_render_time_ms(renderTime);
            frame.set_timestamp(renderTime);
        }
        
        return 1;
    }
    
protected:
    FILE *f_ = NULL;
};
#endif
