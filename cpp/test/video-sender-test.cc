//
//  video-sender-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 9/20/13
//

#include "test-common.h"
#include "video-sender.h"
#include "ndnrtc-utils.h"
#include "frame-buffer.h"
#include <string.h>

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));
#if 1
TEST(VideoSenderParamsTest, CheckDefaults)
{
    ParamsStruct p = DefaultParams;
    
    char *hubEx = (char*)"ndn/edu/ucla/apps";
    char *userEx = (char*)"testuser";
    char *streamEx = (char*)"video0";
    
    char *hub, *user, *stream;
    
    hub = (char*)malloc(256);
    user = (char*)malloc(256);
    stream = (char*)malloc(256);
    
    memset(hub, 0, 256);
    memset(user, 0, 256);
    memset(stream, 0, 256);
    
    EXPECT_STREQ(hubEx, p.ndnHub);
    EXPECT_STREQ(userEx, p.producerId);
    EXPECT_STREQ(streamEx, p.streamName);
    
    char prefix[256];
    sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s", hubEx, userEx, streamEx);
    
    shared_ptr<string> streamPrefix = NdnRtcNamespace::getStreamPrefix(p);
    
    EXPECT_NE(nullptr, streamPrefix.get());
    EXPECT_STREQ(prefix, streamPrefix->c_str());
    
    free(hub);
    free(user);
    free(stream);
}

TEST(VideoSenderParamsTest, CheckPrefixes)
{
    ParamsStruct p = DefaultParams;
    
    const char *hubEx = "ndn/edu/ucla/apps";
    char *userEx = (char*)"testuser";
    char *streamEx = (char*)"video0";
    
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s", hubEx, userEx);
        
        shared_ptr<string> uprefix = NdnRtcNamespace::getUserPrefix(p);
        
        EXPECT_NE(nullptr, uprefix.get());
        EXPECT_STREQ(prefix, uprefix->c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/key", hubEx, userEx, streamEx);
        
        shared_ptr<string> keyprefix = NdnRtcNamespace::getStreamKeyPrefix(p);
        
        EXPECT_NE(nullptr, keyprefix.get());
        EXPECT_STREQ(prefix, keyprefix->c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/vp8/frames/delta", hubEx, userEx, streamEx);
        
        shared_ptr<string> frprefix = NdnRtcNamespace::getStreamFramePrefix(p);
        
        EXPECT_NE(nullptr, frprefix.get());
        EXPECT_TRUE(string(prefix) == *frprefix);
        EXPECT_STREQ(prefix, frprefix->c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/vp8/frames/key", hubEx, userEx, streamEx);
        
        shared_ptr<string> frprefix = NdnRtcNamespace::getStreamFramePrefix(p, true);
        EXPECT_NE(nullptr, frprefix.get());
        EXPECT_STREQ(prefix, frprefix->c_str());
    }
}
#endif
class VideoSenderTester :
public NdnRtcObjectTestHelper,
public UnitTestHelperNdnNetwork
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        dataInbox_.clear();
    
        params_ = DefaultParams;
        params_.host = "localhost";
        params_.freshness = 1;
        
        videoSender_.reset(new NdnVideoSender(params_));
        videoSender_->setObserver(this);
        videoSender_->setLogger(&Logger::sharedInstance());
        
        shared_ptr<std::string> streamAccessPrefix = NdnRtcNamespace::getStreamKeyPrefix(params_);
        shared_ptr<std::string> userPrefix = NdnRtcNamespace::getUserPrefix(params_);
        
        UnitTestHelperNdnNetwork::NdnSetUp(*streamAccessPrefix, *userPrefix);
        
        ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(*userPrefix);
        ndnKeyChain_->setFace(ndnFace_.get());
        sampleFrame_ = NULL;
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (sampleFrame_)
        {
            delete sampleFrame_->_buffer;
            delete sampleFrame_;
        }
        
        UnitTestHelperNdnNetwork::NdnTearDown();
    }
    
    void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, Transport& transport)
    {
        Logger::sharedInstance().log(NdnLoggerLevelTrace)
        << "got interest: " << interest->getName() << endl;
    }
    
    void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        FAIL();
    }
    
    void onData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
    {
        Logger::sharedInstance().log(NdnLoggerLevelTrace)
        << "Got data packet with name " << data->getName()
        << " size: " << data->getContent().size() << endl;
        
        dataInbox_.push_back(data);
        
        dataReceived_ = true;
        ++callbackCount_;
    }
    
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        timeoutReceived_ = true;
        ++callbackCount_;
        
        Logger::sharedInstance().log(NdnLoggerLevelTrace)
        << "Time out for interest " << interest->getName() << endl;
    }
    
protected:
    int callbackCount_;
    bool dataReceived_, timeoutReceived_;
    std::vector<shared_ptr<Data>> dataInbox_;
    shared_ptr<NdnVideoSender> videoSender_;
    webrtc::EncodedImage *sampleFrame_;
    
    void loadFrame()
    {
        int width = 640, height = 480;
        // change frame size according to the data in resource file file
        
        FILE *f = fopen("resources/vp8_640x480.frame", "rb");
        ASSERT_TRUE(f);
        
        int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
        
        
        int32_t size, length;
        ASSERT_TRUE(fread(&size, sizeof(size), 1, f));
        ASSERT_TRUE(fread(&length, sizeof(length), 1, f));
        
        unsigned char* frameData = new unsigned char[size];
        
        ASSERT_TRUE(fread(frameData, 1, length, f));
        
        sampleFrame_ = new webrtc::EncodedImage(frameData, length, size);
        sampleFrame_->_encodedWidth = width;
        sampleFrame_->_encodedHeight = height;
        sampleFrame_->_timeStamp = NdnRtcUtils::millisecondTimestamp();
        sampleFrame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
        
        fclose(f);
    }
    
    virtual void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        
        callbackCount_ = 0;
        dataReceived_ = false;
        timeoutReceived_ = false;
    }
};
#if 1
TEST_F(VideoSenderTester, TestFrameData)
{
    loadFrame();
    int64_t ts = NdnRtcUtils::millisecondTimestamp();
    sampleFrame_->capture_time_ms_ = ts;
    NdnFrameData data(*sampleFrame_);
    
    { // pack frame into network-ready data
        EXPECT_NE(0, data.getLength());
        EXPECT_LT(sampleFrame_->_length, data.getLength());
        
        unsigned int length = data.getLength();
        unsigned char *buf = data.getData();
        webrtc::EncodedImage frame;
        ASSERT_EQ(RESULT_OK, data.getFrame(frame));
        
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &frame);
        
        EXPECT_TRUE(data.isValid());
        EXPECT_EQ(PacketData::TypeVideo, data.getType());
        EXPECT_NE(PacketData::TypeAudio, data.getType());
        EXPECT_EQ(0, data.getMetadata().packetRate_);
        EXPECT_EQ(0, data.getMetadata().timestamp_);
    }
    { // restore frame from raw data
        unsigned char* networkData = data.getData();
        unsigned int dataLength = data.getLength();
        
        NdnFrameData restoredData(dataLength, networkData);
        
        EXPECT_TRUE(restoredData.isValid());
        EXPECT_EQ(dataLength, restoredData.getLength());
        EXPECT_EQ(PacketData::TypeVideo, restoredData.getType());
        EXPECT_NE(PacketData::TypeAudio, restoredData.getType());
        
        webrtc::EncodedImage frame;
        EXPECT_EQ(RESULT_OK, restoredData.getFrame(frame));
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &frame);
    }
    {
        unsigned char* networkData = data.getData();
        unsigned int dataLength = data.getLength();
        
        PacketData *packet;
        ASSERT_EQ(RESULT_OK, PacketData::packetFromRaw(dataLength, networkData, &packet));
        
        EXPECT_TRUE(packet->isValid());
        EXPECT_EQ(PacketData::TypeVideo, packet->getType());
        
        webrtc::EncodedImage frame;
        EXPECT_EQ(RESULT_OK,
                  dynamic_cast<NdnFrameData*>(packet)->getFrame(frame));
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &frame);
        
        delete packet;
    }
    { // corrupted data
        unsigned char* networkData = data.getData();
        unsigned int dataLength = data.getLength();
        
        // corrupt byte 0 - marker
        networkData[0] = 0;
        
        for (int i = 0; i < dataLength/2; i++)
        {
            int pos = NdnRtcObjectTestHelper::randomInt(0, dataLength);
            int val = NdnRtcObjectTestHelper::randomInt(0, 255);
            networkData[pos] = val;
        }
        
        NdnFrameData restoredData(dataLength, networkData);
        
        EXPECT_FALSE(restoredData.isValid());
        EXPECT_EQ(dataLength, restoredData.getLength());
        EXPECT_EQ(-1, restoredData.getMetadata().packetRate_);
        EXPECT_EQ(-1, restoredData.getMetadata().timestamp_);
        
        webrtc::EncodedImage frame;
        EXPECT_EQ(RESULT_ERR, restoredData.getFrame(frame));
    }
}

TEST_F(VideoSenderTester, TestFrameDataWithMetadata)
{
    loadFrame();
    // check different rates
    for (int i = 1; i <= 30; i++)
    {
        PacketData::PacketMetadata metadata = {0., 0}, resMetadata = {0., 0};
        metadata.packetRate_ = (double)i;
        metadata.timestamp_ = NdnRtcUtils::millisecondTimestamp();
        
        NdnFrameData data(*sampleFrame_, metadata);
        
        EXPECT_NE(0, data.getLength());
        EXPECT_LT(sampleFrame_->_length, data.getLength());
        
        unsigned int length = data.getLength();
        unsigned char *buf = data.getData();
        webrtc::EncodedImage frame;
        ASSERT_EQ(RESULT_OK, data.getFrame(frame));
        ASSERT_TRUE(data.isValid());
        
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &frame);
        EXPECT_EQ(metadata.packetRate_, data.getMetadata().packetRate_);
        EXPECT_EQ(metadata.timestamp_, data.getMetadata().timestamp_);
    }
    
    // check different frame numbers
    for (int i = -INT_MAX/100000; i < INT_MAX/10000; i++)
    {
        PacketData::PacketMetadata metadata = {0., 0}, resMetadata = {0., 0};
        metadata.packetRate_ = (double)i;
        metadata.timestamp_ = NdnRtcUtils::millisecondTimestamp();
        
        NdnFrameData data(*sampleFrame_, metadata);
        
        EXPECT_NE(0, data.getLength());
        EXPECT_LT(sampleFrame_->_length, data.getLength());
        
        unsigned int length = data.getLength();
        unsigned char *buf = data.getData();
        webrtc::EncodedImage frame;
        ASSERT_EQ(RESULT_OK, data.getFrame(frame));
        ASSERT_TRUE(data.isValid());
        
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &frame);
        EXPECT_EQ(metadata.packetRate_, data.getMetadata().packetRate_);
        EXPECT_EQ(metadata.timestamp_, data.getMetadata().timestamp_);
    }
}

TEST_F(VideoSenderTester, TestInit)
{
    EXPECT_EQ(0,videoSender_->init(ndnFace_, ndnTransport_));
}

TEST_F(VideoSenderTester, TestSend)
{
    // delay from previous tests - previous objects should be marked stale
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnReceiverFace_, ndnTransport_));
    
    // send frame
    videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    EXPECT_EQ(false, obtainedError_);
    
    // get frame prefix
    shared_ptr<std::string> prefixStr = NdnRtcNamespace::getStreamFramePrefix(params_);
    Name framePrefix(prefixStr->c_str());
    framePrefix.addComponent((const unsigned char *)"0", 1);
    
    // fetch data from ndn
    int waitMs = 1000;
    Interest interest(framePrefix, waitMs);
    interest.setMustBeFresh(true);
    ndnReceiverFace_->expressInterest(interest,
                                     bind(&VideoSenderTester::onData, this, _1, _2),
                                     bind(&VideoSenderTester::onTimeout, this, _1));
    
    startProcessingNdn();
    EXPECT_TRUE_WAIT(dataReceived_, waitMs);
    stopProcessingNdn();

    ASSERT_EQ(1, dataInbox_.size());
    
    shared_ptr<Data> dataObject = dataInbox_[0];
    
    SegmentData segmentData;
    EXPECT_EQ(RESULT_OK,
              SegmentData::segmentDataFromRaw(dataObject->getContent().size(),
                                              dataObject->getContent().buf(),
                                              segmentData));
    EXPECT_TRUE(segmentData.isValid());
    
    NdnFrameData data(segmentData.getSegmentDataLength(),
                      segmentData.getSegmentData());
    webrtc::EncodedImage restoredFrame;
    EXPECT_EQ(RESULT_OK, data.getFrame(restoredFrame));
    NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &restoredFrame);
}

TEST_F(VideoSenderTester, TestSendSeveralFrames)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1200);
    
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnReceiverFace_, ndnTransport_));
    
    // send frame
    int framesNum = 10;
    
    for (int i = 0; i < framesNum; i++)
        videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    
    EXPECT_EQ(false, obtainedError_);
    
    // get frame prefix
    shared_ptr<std::string> prefixStr = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    Name framesPrefix(prefixStr->c_str());
    
    for (int i = 0; i < framesNum; i++)
    {
        Name framePrefix = framesPrefix;
        framePrefix.append(NdnRtcUtils::componentFromInt(i));
        
        Interest interest(framePrefix, 1000);
        interest.setMustBeFresh(true);
        ndnFace_->expressInterest(interest, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    
    startProcessingNdn();
    int waitTime = 10;
    WAIT(waitTime);
    stopProcessingNdn();
    
    EXPECT_TRUE(dataReceived_);
    EXPECT_EQ(framesNum, dataInbox_.size());
    
    for (int i = 0; i < dataInbox_.size(); i++)
    {
        shared_ptr<Data> dataObject = dataInbox_[i];
        webrtc::EncodedImage frame;
        
        SegmentData segmentData;
        EXPECT_EQ(RESULT_OK,
                  SegmentData::segmentDataFromRaw(dataObject->getContent().size(),
                                                  dataObject->getContent().buf(),
                                                  segmentData));
        EXPECT_TRUE(segmentData.isValid());
        
        NdnFrameData data(segmentData.getSegmentDataLength(),
                          segmentData.getSegmentData());
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, data.getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &restoredFrame);
    }
}
#endif
TEST_F(VideoSenderTester, TestSendBySegments)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1200);
    
    loadFrame();
    // set segment size smaller than frame
    int segmentSize = sampleFrame_->_length/3;
    int segmentNum = (sampleFrame_->_length-3*segmentSize)?4:3;
    
    params_.segmentSize = segmentSize;
    
    videoSender_.reset(new NdnVideoSender(params_));
    videoSender_->setObserver(this);
    
    EXPECT_EQ(0,videoSender_->init(ndnFace_, ndnTransport_));
    
    // send frame
    videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    EXPECT_EQ(false, obtainedError_);
    
    // get frame prefix
    shared_ptr<std::string> prefixStr = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    Name framePrefix(prefixStr->c_str());
    
    framePrefix.append(NdnRtcUtils::componentFromInt(0));
    NdnRtcNamespace::appendDataKind(framePrefix, false);
    
    int waitMs = 10000;
    // send out interests for each segment
    for (int sno = 0; sno < segmentNum; sno++)
    {
        Name segmentPrefix = framePrefix;

        segmentPrefix.appendSegment(sno);
        
        // fetch data from ndn
        Interest interest(segmentPrefix, waitMs);
        interest.setMustBeFresh(true);
        ndnFace_->expressInterest(interest, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    
    startProcessingNdn();
    EXPECT_TRUE_WAIT(callbackCount_ == segmentNum, 2000);
    stopProcessingNdn();
    
    EXPECT_TRUE(dataReceived_);
    ASSERT_EQ(segmentNum, dataInbox_.size());
    
    // now assemble frame
    unsigned char frameBuf[80000];
    unsigned char *pos = &frameBuf[0];
    
    for (int sno = 0; sno < segmentNum; sno++)
    {
        for (int i = 0; i < dataInbox_.size(); i++)
        {
            shared_ptr<Data> segmentData = dataInbox_[i];
            
            if (framePrefix.match(segmentData->getName()))
            {
                int ncomp = framePrefix.getComponentCount();
                Name::Component segmentComp = segmentData->getName().getComponent(ncomp);
                
                // decode segment component into a segment number
                if (sno == NdnRtcUtils::segmentNumber(segmentComp))
                {
                    memcpy(pos,
                           segmentData->getContent().buf()+SegmentData::getHeaderSize(),
                           segmentData->getContent().size()-SegmentData::getHeaderSize());
              
                    pos += segmentData->getContent().size()-SegmentData::getHeaderSize();
                    break;
                }
            } // prefix match
        } // search for a segment
    } // segments number
    
    NdnFrameData data(pos-&frameBuf[0], &frameBuf[0]);
    webrtc::EncodedImage restoredFrame;
    ASSERT_EQ(RESULT_OK, data.getFrame(restoredFrame));
    NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &restoredFrame);
    
}
#if 1
TEST_F(VideoSenderTester, TestSendWithLibException)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1000);
    
    loadFrame();
    
    videoSender_->setObserver(this);
    
    EXPECT_EQ(0,videoSender_->init(ndnFace_, ndnTransport_));
    
    ndnTransport_->close();
    
    // send frame
    videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    EXPECT_EQ(true, obtainedError_);
}

TEST_F(VideoSenderTester, TestSendInTwoNamespaces)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1200);
    
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnReceiverFace_, ndnTransport_));
    
    // send frame
    int deltaFramesNum = 3, keyFramesNum = 3;
    
    for (int i = 0; i < deltaFramesNum; i++)
        videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    
    sampleFrame_->_frameType = webrtc::kKeyFrame;
    for (int i = 0; i < keyFramesNum; i++)
        videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    
    EXPECT_EQ(false, obtainedError_);
    
    // get frame prefix
    shared_ptr<std::string> deltaPrefixStr = NdnRtcNamespace::getStreamFramePrefix(params_),
    keyPrefixStr = NdnRtcNamespace::getStreamFramePrefix(params_, true);

    EXPECT_NE(nullptr, deltaPrefixStr);
    EXPECT_NE(nullptr, keyPrefixStr);
    
    Name deltaFramesPrefix(deltaPrefixStr->c_str());
    Name keyFramesPrefix(keyPrefixStr->c_str());
    
    // fetch delta frames
    for (int i = 0; i < deltaFramesNum; i++)
    {
        Name framePrefix = deltaFramesPrefix;

        framePrefix.append(NdnRtcUtils::componentFromInt(i));
        NdnRtcNamespace::appendDataKind(framePrefix, false);
        framePrefix.appendSegment(0);
        
        Interest interest(framePrefix, 3000);
        interest.setMustBeFresh(true);
        ndnFace_->expressInterest(interest, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    WAIT(200);
    // fetch key frames
    for (int i = 0; i < keyFramesNum; i++)
    {
        Name framePrefix = keyFramesPrefix;
        framePrefix.append(NdnRtcUtils::componentFromInt(i));
        NdnRtcNamespace::appendDataKind(framePrefix, false);
        framePrefix.appendSegment(0);
        
        Interest interest(framePrefix, 3000);
        interest.setMustBeFresh(true);
        ndnFace_->expressInterest(interest, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    
    startProcessingNdn();
    EXPECT_TRUE_WAIT((callbackCount_ == deltaFramesNum+keyFramesNum || timeoutReceived_), 10000);
    stopProcessingNdn();
    
    EXPECT_TRUE(dataReceived_);
    EXPECT_EQ(deltaFramesNum+keyFramesNum, dataInbox_.size());
    
    for (int i = 0; i < dataInbox_.size(); i++)
    {
        shared_ptr<Data> dataObject = dataInbox_[i];

        SegmentData segmentData;
        EXPECT_EQ(RESULT_OK,
                  SegmentData::segmentDataFromRaw(dataObject->getContent().size(),
                                                  dataObject->getContent().buf(),
                                                  segmentData));
        EXPECT_TRUE(segmentData.isValid());
        
        NdnFrameData data(segmentData.getSegmentDataLength(),
                          segmentData.getSegmentData());
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, data.getFrame(restoredFrame));
        
        // set frame type field accroding to the frame type
        if (i < deltaFramesNum)
            sampleFrame_->_frameType = webrtc::kDeltaFrame;
        else
            sampleFrame_->_frameType = webrtc::kKeyFrame;
        
        NdnRtcObjectTestHelper::checkFrames(sampleFrame_, &restoredFrame);
    }
}
#endif