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

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR

//#define NDN_DETAILED
//#define NDN_TRACE
//#define NDN_DEBUG

#include "test-common.h"
#include "video-sender.h"
#include "ndnrtc-utils.h"
#include <string.h>

using namespace ndnrtc;

TEST(VideoSenderParamsTest, CreateDelete)
{
    VideoSenderParams *p = VideoSenderParams::defaultParams();
    delete p;
}

TEST(VideoSenderParamsTest, CheckDefaults)
{
    VideoSenderParams *p = VideoSenderParams::defaultParams();
    
    char *hubEx = "ndn";
    char *userEx = "testuser";
    char *streamEx = "video0";
    
    char *hub, *user, *stream;
    
    hub = (char*)malloc(256);
    user = (char*)malloc(256);
    stream = (char*)malloc(256);

    memset(hub, 0, 256);
    memset(user, 0, 256);
    memset(stream, 0, 256);
    
    EXPECT_EQ(0, p->getHub((char**)&hub));
    EXPECT_STREQ(hubEx, hub);
    
    EXPECT_EQ(0, p->getProducerId((char**)&user));
    EXPECT_STREQ(userEx, user);
    
    EXPECT_EQ(0, p->getStreamName((char**)&stream));
    EXPECT_STREQ(streamEx, stream);
    
    char prefix[256];
    sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s", hubEx, userEx, streamEx);
    
    EXPECT_STREQ(prefix, p->getStreamPrefix().c_str());
    
    free(hub);
    free(user);
    free(stream);
    
    delete p;
}

TEST(VideoSenderParamsTest, CheckPrefixes)
{
    VideoSenderParams *p = VideoSenderParams::defaultParams();
    
    char *hubEx = "ndn";
    char *userEx = "testuser";
    char *streamEx = "video0";
    
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/key", hubEx, userEx, streamEx);
        
        EXPECT_STREQ(prefix, p->getStreamKeyPrefix().c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/vp8-640/frames", hubEx, userEx, streamEx);
        
        EXPECT_STREQ(prefix, p->getStreamFramePrefix().c_str());
    }
    
    delete p;
}

class VideoSenderTester : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        dataInbox_.clear();
        
        shared_ptr<Transport::ConnectionInfo> connInfo(new TcpTransport::ConnectionInfo("localhost"));
        
        params_.reset(VideoSenderParams::defaultParams());
        videoSender_.reset(new NdnVideoSender(params_.get()));
        videoSender_->setObserver(this);
        
        ndnTransport_.reset(new TcpTransport());
        ndnFace_.reset(new Face(ndnTransport_, connInfo));
        
        std::string streamAccessPrefix = params_.get()->getStreamKeyPrefix();
        
        ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                bind(&VideoSenderTester::onInterest, this, _1, _2, _3),
                                bind(&VideoSenderTester::onRegisterFailed, this, _1));
        
        shared_ptr<PrivateKeyStorage> privateKeyStorage(new PrivateKeyStorage());
        shared_ptr<IdentityManager> identityManager(new IdentityManager(privateKeyStorage));

        ndnKeyChain_.reset(new KeyChain(identityManager));
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
        
        ndnFace_.reset();
        ndnTransport_.reset();
        params_.reset();
        videoSender_.reset();
        ndnKeyChain_.reset();
    }
    
    void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, Transport& transport)
    {
        INFO("got interest: %s", interest->getName().toUri().c_str());
    }
    
    void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        FAIL();
    }
    
    void onData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
    {
        INFO("Got data packet with name %s, size: %d", data->getName().to_uri().c_str(), data->getContent().size());
        dataInbox_.push_back(data);
        
        dataReceived_ = true;
        ++callbackCount_;
    }
    
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        timeoutReceived_ = true;
        ++callbackCount_;
        INFO("Time out for interest %s", interest->getName().toUri().c_str());
    }
    
protected:
    int callbackCount_;
    bool dataReceived_, timeoutReceived_;
    
    std::vector<shared_ptr<Data>> dataInbox_;
    
    shared_ptr<TcpTransport> ndnTransport_;
    shared_ptr<Face> ndnFace_;
    shared_ptr<VideoSenderParams> params_;
    shared_ptr<NdnVideoSender> videoSender_;
    shared_ptr<KeyChain> ndnKeyChain_;
    
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
        
#warning - temp hack (size instead of length)
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

TEST_F(VideoSenderTester, TestFrameData)
{
    loadFrame();
    
    NdnFrameData data(*sampleFrame_);
    
    EXPECT_NE(0, data.getLength());
    EXPECT_LT(sampleFrame_->_length, data.getLength());
    
    unsigned int length = data.getLength();
    unsigned char *buf = data.getData();
    webrtc::EncodedImage *frame = nullptr;
    ASSERT_EQ(0, NdnFrameData::unpackFrame(length, buf, &frame));

    ASSERT_NE(nullptr, frame);
    
    NdnRtcObjectTestHelper::checkFrames(sampleFrame_, frame);
}

TEST_F(VideoSenderTester, TestInit)
{
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnTransport_, ndnKeyChain_));
}

TEST_F(VideoSenderTester, TestSend)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1000);
    
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnTransport_, ndnKeyChain_));

    // send frame
    videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    EXPECT_EQ(false, obtainedError_);

    // just to be sure - wait for ndn network to poke the object
//    WAIT(1000);
    
    // get frame prefix
    std::string prefixStr = params_->getStreamFramePrefix();
    Name framePrefix(prefixStr.c_str());
    
    framePrefix.addComponent((const unsigned char *)"0", 1);
    framePrefix.appendSegment(0);
    
    // fetch data from ndn
    long long waitMs = 10000;
    Interest interest(framePrefix, waitMs);
    ndnFace_->expressInterest(interest, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    
    while (!(dataReceived_ || timeoutReceived_) && waitMs > 0)
    {
        ndnFace_->processEvents();
        usleep(10000);
        waitMs -= 10;
    }

    EXPECT_TRUE(dataReceived_);
    ASSERT_EQ(1, dataInbox_.size());

    shared_ptr<Data> dataObject = dataInbox_[0];
    webrtc::EncodedImage *frame = nullptr;

    ASSERT_EQ(0,NdnFrameData::unpackFrame(dataObject->getContent().size(), dataObject->getContent().buf(), &frame));
    ASSERT_NE(nullptr, frame);
    
#if 0
    if (sampleFrame_->_encodedWidth != frame->_encodedWidth)
    {
        INFO("wrong");
    }
#endif 
    
    EXPECT_EQ(sampleFrame_->_encodedWidth, frame->_encodedWidth);
    EXPECT_EQ(sampleFrame_->_encodedHeight, frame->_encodedHeight);
    EXPECT_EQ(sampleFrame_->_frameType, frame->_frameType);
    EXPECT_EQ(sampleFrame_->_completeFrame, frame->_completeFrame);
    
    delete frame;
}

TEST_F(VideoSenderTester, TestSendSeveralFrames)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1000);
    
    loadFrame();
    
    EXPECT_EQ(0,videoSender_->init(ndnTransport_, ndnKeyChain_));
    
    // send frame
    int framesNum = 3;
    
    for (int i = 0; i < framesNum; i++)
        videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    
    EXPECT_EQ(false, obtainedError_);
    
    // just to be sure - wait for ndn network to poke the object
    //    WAIT(1000);
    
    // get frame prefix
    std::string prefixStr = params_->getStreamFramePrefix();
    Name framesPrefix(prefixStr.c_str());
    
    for (int i = 0; i < framesNum; i++)
    {
        Name framePrefix = framesPrefix;
        
        char frameNoStr[3];
        memset(&frameNoStr[0], 0, 3);
        sprintf(&frameNoStr[0], "%d", i);
        
        framePrefix.addComponent((const unsigned char *)&frameNoStr[0], strlen(frameNoStr));
        framePrefix.appendSegment(0);
        ndnFace_->expressInterest(framePrefix, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    
    // fetch data from ndn
    long long waitMs = 10000;
    while (!(callbackCount_ == 3 || timeoutReceived_) && waitMs > 0)
    {
        ndnFace_->processEvents();
        usleep(10000);
        waitMs -= 10;
    }
    
    EXPECT_TRUE(dataReceived_);
    EXPECT_EQ(3, dataInbox_.size());
    
    for (int i = 0; i < dataInbox_.size(); i++)
    {
        shared_ptr<Data> dataObject = dataInbox_[i];
        webrtc::EncodedImage *frame = nullptr;
        
        ASSERT_EQ(0, NdnFrameData::unpackFrame(dataObject->getContent().size(), dataObject->getContent().buf(), &frame));
        ASSERT_NE(nullptr, frame);
        
#if 0
        if (sampleFrame_->_encodedWidth != frame->_encodedWidth)
        {
            INFO("wrong");
        }
#endif
        
        EXPECT_EQ(sampleFrame_->_encodedWidth, frame->_encodedWidth);
        EXPECT_EQ(sampleFrame_->_encodedHeight, frame->_encodedHeight);
        EXPECT_EQ(sampleFrame_->_frameType, frame->_frameType);
        EXPECT_EQ(sampleFrame_->_completeFrame, frame->_completeFrame);
        delete frame;
    }
}

TEST_F(VideoSenderTester, TestSendBySegments)
{
    // delay from previous tests - previous objects should be marked stale
    WAIT(1000);
    
    loadFrame();
    // set segment size smaller than frame
    int segmentSize = sampleFrame_->_length/3;
    int segmentNum = (sampleFrame_->_length-3*segmentSize)?4:3;
    
    params_->setIntParam(VideoSenderParams::ParamNameSegmentSize, segmentSize);
    
    videoSender_.reset(new NdnVideoSender(params_.get()));
    videoSender_->setObserver(this);
    
    EXPECT_EQ(0,videoSender_->init(ndnTransport_, ndnKeyChain_));
    
    // send frame
    videoSender_->onEncodedFrameDelivered(*sampleFrame_);
    EXPECT_EQ(false, obtainedError_);
    
    // get frame prefix
    std::string prefixStr = params_->getStreamFramePrefix();
    Name framePrefix(prefixStr.c_str());
    
    framePrefix.addComponent((const unsigned char *)"0", 1);
    
    long long waitMs = 10000;
    // send out interests for each segment
    for (int sno = 0; sno < segmentNum; sno++)
    {
        Name segmentPrefix = framePrefix;

        segmentPrefix.appendSegment(sno);
        
        // fetch data from ndn
        ndnFace_->expressInterest(segmentPrefix, bind(&VideoSenderTester::onData, this, _1, _2), bind(&VideoSenderTester::onTimeout, this, _1));
    }
    
    // wait for data
    while (!(callbackCount_ == segmentNum || timeoutReceived_) && waitMs > 0)
    {
        ndnFace_->processEvents();
        usleep(10000);
        waitMs -= 10;
    }
    
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
                int ncomp = segmentData->getName().getComponentCount();
                Name::Component segmentComp = segmentData->getName().getComponent(ncomp-1);
                
                // decode segment component into a segment number
                if (sno == NdnRtcUtils::segmentNumber(segmentComp))
                {
                    memcpy(pos, segmentData->getContent().buf(), segmentData->getContent().size());
                    pos += segmentData->getContent().size();
                    break;
                }
            } // prefix match
        } // search for a segment
    } // segments number
    
    webrtc::EncodedImage *frame = nullptr;
    
    ASSERT_EQ(0,NdnFrameData::unpackFrame(pos-&frameBuf[0], &frameBuf[0], &frame));
    ASSERT_NE(nullptr, frame);
    
#if 0
    if (sampleFrame_->_encodedWidth != frame->_encodedWidth)
    {
        INFO("wrong");
    }
#endif
    
    EXPECT_EQ(sampleFrame_->_encodedWidth, frame->_encodedWidth);
    EXPECT_EQ(sampleFrame_->_encodedHeight, frame->_encodedHeight);
    EXPECT_EQ(sampleFrame_->_frameType, frame->_frameType);
    EXPECT_EQ(sampleFrame_->_completeFrame, frame->_completeFrame);
    
    delete frame;
}
