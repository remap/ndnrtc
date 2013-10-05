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
usleep(100000); \
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

class NdnRtcObjectTestHelper : public ::testing::Test, public ndnrtc::INdnRtcObjectObserver
{
public:
    virtual void SetUp()
    {
        flushFlags();
#ifdef WEBRTC_LOGGING
        setupWebRTCLogging();
#endif
    }
    virtual void TearDown()
    {
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
//        ASSERT_TRUE(f);
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
        webrtc::Trace::SetTraceFile("bin/webrtc.txt");
//        webrtc::Trace::SetLevelFilter(webrtc::kTraceAll);
    }
};

class CocoaTestEnvironment : public ::testing::Environment
{
public:
    void SetUp();
    void TearDown();
    
protected:
    void *pool_;
};

#endif
