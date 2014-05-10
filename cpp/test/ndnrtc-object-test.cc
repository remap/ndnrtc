//
//  ndnrtc-object-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "gtest/gtest.h"
#include "simple-log.h"
#include "ndnrtc-object.h"
#include "test-common.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(ParamsTest, TestValidateVideoParams)
{
    {
        ParamsStruct p = DefaultParams;
        EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
    }
    { // check failures
        { // log file
            ParamsStruct p = DefaultParams;
            p.logFile = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.logFile = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.logFile = "test-file";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
        { // host
            ParamsStruct p = DefaultParams;
            p.host = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.host = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.host = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
        { // producerId
            ParamsStruct p = DefaultParams;
            p.producerId = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.producerId = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.producerId = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
        { // stream name
            ParamsStruct p = DefaultParams;
            p.streamName = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.streamName = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.streamName = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
        { // stream thread
            ParamsStruct p = DefaultParams;
            p.streamThread = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.streamThread = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.streamThread = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
        { // ndnhub
            ParamsStruct p = DefaultParams;
            p.ndnHub = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.ndnHub = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateVideoParams(p, p));
            p.ndnHub = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateVideoParams(p, p));
        }
    }
    
    { // check warnings
        {
            ParamsStruct p = DefaultParams;
            p.captureWidth = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.captureWidth, p.captureWidth);
        }
        {
            ParamsStruct p = DefaultParams;
            p.captureHeight = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.captureHeight, p.captureHeight);
        }
        {
            ParamsStruct p = DefaultParams;
            p.captureFramerate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.captureFramerate, p.captureFramerate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.renderWidth = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.renderWidth, p.renderWidth);
        }
        {
            ParamsStruct p = DefaultParams;
            p.renderHeight = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.renderHeight, p.renderHeight);
        }
        {
            ParamsStruct p = DefaultParams;
            p.codecFrameRate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.codecFrameRate, p.codecFrameRate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.startBitrate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.startBitrate, p.startBitrate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.maxBitrate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.maxBitrate, p.maxBitrate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.encodeWidth = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.encodeWidth, p.encodeWidth);
        }
        {
            ParamsStruct p = DefaultParams;
            p.encodeHeight = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.encodeHeight, p.encodeHeight);
        }
        {
            ParamsStruct p = DefaultParams;
            p.portNum = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.portNum, p.portNum);
        }
        {
            ParamsStruct p = DefaultParams;
            p.segmentSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.segmentSize, p.segmentSize);
        }
        {
            ParamsStruct p = DefaultParams;
            p.producerRate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.producerRate, p.producerRate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.playbackRate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.playbackRate, p.playbackRate);
        }
        {
            ParamsStruct p = DefaultParams;
            p.bufferSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.bufferSize, p.bufferSize);
        }
        {
            ParamsStruct p = DefaultParams;
            p.slotSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateVideoParams(p, p));
            EXPECT_EQ(DefaultParams.slotSize, p.slotSize);
        }
    }
}

TEST(ParamsTest, TestValidateAudioParams)
{
    {
        ParamsStruct p = DefaultParamsAudio;
        EXPECT_EQ(RESULT_OK, ParamsStruct::validateAudioParams(p, p));
    }
    { // check failures
        { // host
            ParamsStruct p = DefaultParamsAudio;
            p.host = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.host = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.host = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateAudioParams(p, p));
        }
        { // producerId
            ParamsStruct p = DefaultParamsAudio;
            p.producerId = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.producerId = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.producerId = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateAudioParams(p, p));
        }
        { // stream name
            ParamsStruct p = DefaultParamsAudio;
            p.streamName = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.streamName = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.streamName = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateAudioParams(p, p));
        }
        { // stream thread
            ParamsStruct p = DefaultParamsAudio;
            p.streamThread = NULL;
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.streamThread = "";
            EXPECT_EQ(RESULT_ERR, ParamsStruct::validateAudioParams(p, p));
            p.streamThread = "test";
            EXPECT_EQ(RESULT_OK, ParamsStruct::validateAudioParams(p, p));
        }
    }
    { // check warnings
        {
            ParamsStruct p = DefaultParamsAudio;
            p.portNum = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.portNum, p.portNum);
        }
        {
            ParamsStruct p = DefaultParamsAudio;
            p.segmentSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.segmentSize, p.segmentSize);
        }
        {
            ParamsStruct p = DefaultParamsAudio;
            p.producerRate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.producerRate, p.producerRate);
        }
        {
            ParamsStruct p = DefaultParamsAudio;
            p.playbackRate = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.playbackRate, p.playbackRate);
        }
        {
            ParamsStruct p = DefaultParamsAudio;
            p.bufferSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.bufferSize, p.bufferSize);
        }
        {
            ParamsStruct p = DefaultParamsAudio;
            p.slotSize = 999999;
            EXPECT_EQ(RESULT_WARN, ParamsStruct::validateAudioParams(p, p));
            EXPECT_EQ(DefaultParamsAudio.slotSize, p.slotSize);
        }
    }
}

//********************************************************************************
/**
 * @name NdnrtcObject class tests
 */
class NdnRtcObjectTester : public NdnRtcObject
{
public:
    int postError(int code, char *msg) {
        return notifyError(code, msg);
    };
    int postErrorNoParams(){
        return notifyErrorNoParams();
    };
    int postErrorBadArg(const char *pname){
        return notifyErrorBadArg(pname);
    };
    bool isObserved() { return hasObserver(); };
};

class NdnRtcObjectTest : public ::testing::Test, public INdnRtcObjectObserver
{
public:
    void onErrorOccurred(const char *errorMessage){
        errorOccurred_ = true;
        errorMessage_ = (char*)errorMessage;
    };
    
protected:
    bool errorOccurred_ = false;
    char *errorMessage_ = NULL;
};

TEST_F(NdnRtcObjectTest, CreateDelete)
{
    NdnRtcObject *obj = new NdnRtcObject();
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteNoParams)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject();
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteWithParams)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams);
    
    delete obj;
}
TEST_F(NdnRtcObjectTest, CreateDeleteWithObserver)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams, this);
    
    EXPECT_TRUE(obj->isObserved());
    
    delete obj;
}

TEST_F(NdnRtcObjectTest, SetObserver)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject();
    
    EXPECT_FALSE(obj->isObserved());
    obj->setObserver(this);
    EXPECT_TRUE(obj->isObserved());
    
    delete obj;
}

TEST_F(NdnRtcObjectTest, ErrorNotifies)
{
    NdnRtcObjectTester *obj = ( NdnRtcObjectTester *) new NdnRtcObject(DefaultParams, this);
    
    EXPECT_FALSE(errorOccurred_);
    obj->postError(-1, (char*)"error msg");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_STREQ("error msg", errorMessage_);

    errorOccurred_ = false;
    obj->postError(-1, (char*)"");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_STREQ("", errorMessage_);
    
    errorOccurred_ = false;
    obj->postErrorNoParams();
    EXPECT_TRUE(errorOccurred_);

    errorOccurred_ = false;
    obj->postErrorBadArg("value1");
    EXPECT_TRUE(errorOccurred_);
    EXPECT_NE(nullptr, strstr(errorMessage_, "value1"));    
    
    delete obj;
}
