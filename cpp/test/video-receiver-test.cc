//
//  video-receiver-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "video-receiver.h"
#include "ndnrtc-namespace.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

//********************************************************************************
// VideoReceiver tests
class NdnReceiverTester :
public NdnRtcObjectTestHelper,
public IEncodedFrameConsumer,
public UnitTestHelperNdnNetwork
{
public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        params_ = DefaultParams;
        
        string userPrefix = "", streamAccessPrefix = "";
        ASSERT_EQ(RESULT_OK, MediaSender::getUserPrefix(params_, userPrefix));
        ASSERT_EQ(RESULT_OK, MediaSender::getStreamKeyPrefix(params_, streamAccessPrefix));
        
        UnitTestHelperNdnNetwork::NdnSetUp(streamAccessPrefix, userPrefix);
        
        frame_ = NdnRtcObjectTestHelper::loadEncodedFrame();
        payload = new NdnFrameData(*frame_);
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        UnitTestHelperNdnNetwork::NdnTearDown();
    }
    
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        receivedFramesTypes_.push_back(encodedImage._frameType);
        
        receivedFrames_.push_back(encodedImage.capture_time_ms_);
        LOG_TRACE("fetched encoded frame. queue size %d", receivedFrames_.size());
    }
    
    void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        
        fetchedFrames_.clear();
        sentFrames_.clear();
        receivedFrames_.clear();
    }
    
protected:
    std::vector<webrtc::EncodedImage*> fetchedFrames_;
    std::vector<uint64_t> sentFrames_;
    std::vector<uint64_t> receivedFrames_;
    std::vector<int> receivedFramesTypes_;
    webrtc::EncodedImage *frame_;
    NdnFrameData *payload;
    
    void publishMediaPacket(unsigned int dataLen, unsigned char *dataPacket,
                            unsigned int frameNo, unsigned int segmentSize,
                            const string &framePrefix, int freshness,
                            bool mixedSendOrder)
    {
        UnitTestHelperNdnNetwork::publishMediaPacket(dataLen, dataPacket,
                                                     frameNo, segmentSize,
                                                     framePrefix, freshness,
                                                     mixedSendOrder);
        sentFrames_.push_back(millisecondTimestamp());
    }
};
#if 0
TEST_F(NdnReceiverTester, CreateDelete)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(DefaultParams);
    delete receiver;
}

TEST_F(NdnReceiverTester, WrongCallSequences)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(DefaultParams);

    receiver->setObserver(this);
    flushFlags();
    { // call stop
        EXPECT_EQ(RESULT_ERR, receiver->stopFetching());
        EXPECT_TRUE(obtainedError_);
    }

    flushFlags();
    { // call start
        EXPECT_EQ(RESULT_ERR, receiver->startFetching());
        EXPECT_TRUE(obtainedError_);
    }
    flushFlags();
    { // call stop without start
        EXPECT_EQ(RESULT_OK, receiver->init(ndnFace_));
        WAIT(100);
        EXPECT_EQ(RESULT_ERR, receiver->stopFetching());
        WAIT(100);
        EXPECT_TRUE(obtainedError_);
    }
    delete receiver;
}

TEST_F(NdnReceiverTester, Init)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    WAIT(100);
    
    delete receiver;
}

TEST_F(NdnReceiverTester, EmptyFetching)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    EXPECT_EQ(0,receiver->startFetching());
    
    WAIT(100);
    
    EXPECT_EQ(0,receiver->stopFetching());
    
    delete receiver;
}

TEST_F(NdnReceiverTester, Fetching30FPS)
{
    unsigned int jitterSize = 20;
    unsigned int gop = 60;
    unsigned int framesNum = 30;
    unsigned int segmentSize = 100;
    unsigned int bufferSize = gop*2;
    unsigned int producerFrameRate = 30;
    
    params_.bufferSize = bufferSize;
    params_.segmentSize = segmentSize;
    params_.producerRate = producerFrameRate;
    params_.producerId = "testuser1";
    
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    receiver->setLogger(NdnLogger::sharedInstance());
    
    receiver->setFrameConsumer(this);
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    LOG_TRACE("start fetching");
    EXPECT_EQ(0, receiver->startFetching());
    
    // we should start publishing frames later, so that receiver will get first frame
    // by issuing interest with RightMostChild selector
    WAIT(100);
    string framePrefix;
    EXPECT_EQ(RESULT_OK, MediaSender::getStreamFramePrefix(params_, framePrefix));
    
    // publish more than should - depending on RTT value, fetching will not
    // start right from frame 0
    for (int i = 0; i < 2*framesNum; i++)
    {
        frame_->_frameType = (i%gop)?webrtc::kDeltaFrame:webrtc::kKeyFrame;
        frame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
        PacketData::PacketMetadata meta;
        meta.sequencePacketNumber_ = i;
        meta.packetRate_ = producerFrameRate;
        payload = new NdnFrameData(*frame_, meta);
        publishMediaPacket(payload->getLength(), payload->getData(), i,
                           segmentSize, framePrefix, params_.freshness, true);
        WAIT(1000/producerFrameRate);
    }
    EXPECT_TRUE_WAIT(receivedFrames_.size() >= framesNum, 5000);
    cout << "Fetched " << receivedFrames_.size() << " frames" << endl;
    
    {
        for (int i = 0; i < receivedFrames_.size(); i++)
        {
            // check arrival order
            if (i != receivedFrames_.size()-1)
                EXPECT_LE(receivedFrames_[i], receivedFrames_[i+1]);
        }
    }

    receiver->stopFetching();
    delete receiver;
    WAIT(params_.freshness*1000);
}

TEST_F(NdnReceiverTester, Fetching1Segment30FPS)
{
    unsigned int jitterSize = 20;
    unsigned int gop = 60;
    unsigned int framesNum = 30;
    unsigned int segmentSize = 1000;
    unsigned int bufferSize = gop*2;
    unsigned int producerFrameRate = 30;
    
    params_.bufferSize = bufferSize;
    params_.segmentSize = segmentSize;
    params_.producerRate = producerFrameRate;
    params_.producerId = "testuser2";
    
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    receiver->setFrameConsumer(this);
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    LOG_TRACE("start fetching");
    receiver->startFetching();
    
    // we should start publishing frames later, so that receiver will get first frame
    // by issuing interest with RightMostChild selector
    WAIT(100);
    string framePrefix;
    EXPECT_EQ(RESULT_OK, MediaSender::getStreamFramePrefix(params_, framePrefix));

    for (int i = 0; i < 2*framesNum; i++)
    {
        frame_->_frameType = (i%gop)?webrtc::kDeltaFrame:webrtc::kKeyFrame;
        frame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
        PacketData::PacketMetadata meta;
        meta.sequencePacketNumber_ = i;
        meta.packetRate_ = producerFrameRate;
        payload = new NdnFrameData(*frame_, meta);

        publishMediaPacket(payload->getLength(), payload->getData(), i,
                           segmentSize, framePrefix, params_.freshness, true);
        LOG_TRACE("published frame %d",i,5);
        WAIT(1000/producerFrameRate);
    }
    
    EXPECT_TRUE_WAIT(receivedFrames_.size() >= framesNum, 5000);
    cout << "Fetched " << receivedFrames_.size() << " frames" << endl;

    {
        for (int i = 0; i < receivedFrames_.size(); i++)
            if (i != receivedFrames_.size()-1)
                EXPECT_LE(receivedFrames_[i], receivedFrames_[i+1]);
    }
    
    receiver->stopFetching();
    
    delete receiver;
    WAIT(params_.freshness*1000);
}
#endif
TEST_F(NdnReceiverTester, FetchingWithRecovery)
{
    unsigned int jitterSize = 20;
    unsigned int gop = 60;
    unsigned int framesNum = 60;
    unsigned int segmentSize = 100;
    unsigned int bufferSize = gop*2;
    unsigned int producerFrameRate = 30;
    
    params_.bufferSize = bufferSize;
    params_.segmentSize = segmentSize;
    params_.producerRate = producerFrameRate;
    params_.freshness = 1;
    params_.interestTimeout = 2;
    params_.producerId = "testuser3";
    
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    receiver->setLogger(NdnLogger::sharedInstance());
    receiver->setFrameConsumer(this);
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    LOG_TRACE("start fetching");
    EXPECT_EQ(0, receiver->startFetching());
    string framePrefix;
    EXPECT_EQ(RESULT_OK, MediaSender::getStreamFramePrefix(params_, framePrefix));

    PacketData::PacketMetadata meta;
    meta.packetRate_ = 30;
    
    // we should start publishing frames later, so that receiver will get first frame
    // by issuing interest with RightMostChild selector
    WAIT(1000);
    {
        for (int i = 0; i < framesNum; i++)
        {
            frame_->_frameType = (i%gop)?webrtc::kDeltaFrame:webrtc::kKeyFrame;
            frame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
            PacketData::PacketMetadata meta;
            meta.sequencePacketNumber_ = i;
            meta.packetRate_ = producerFrameRate;
            payload = new NdnFrameData(*frame_, meta);

            publishMediaPacket(payload->getLength(), payload->getData(), i,
                               segmentSize, framePrefix, params_.freshness, true);
            LOG_TRACE("published frame %d",i);
            WAIT(1000/producerFrameRate);
        }
        
        EXPECT_TRUE_WAIT(receivedFrames_.size() >= framesNum/2, 5000);
        
        {
            for (int i = 0; i < receivedFrames_.size(); i++)
            {
                if (i != receivedFrames_.size()-1)
                    EXPECT_LE(receivedFrames_[i], receivedFrames_[i+1]);
            }
        }
    }
    
    cout << "Fetched " << receivedFrames_.size() << " frames" << endl;
    flushFlags();
    
    // now let's wait a bit so receiver can switch back to chasing mode
    WAIT(RebufferThreshold+params_.interestTimeout*1000);
    
    { // now publish again
        for (int i = 0; i < framesNum; i++)
        {
            frame_->_frameType = (i%gop)?webrtc::kDeltaFrame:webrtc::kKeyFrame;
            frame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
            PacketData::PacketMetadata meta;
            meta.sequencePacketNumber_ = i;
            meta.packetRate_ = producerFrameRate;
            payload = new NdnFrameData(*frame_, meta);

            publishMediaPacket(payload->getLength(), payload->getData(), i,
                               segmentSize, framePrefix, params_.freshness, true);
            LOG_TRACE("published frame %d",i);
            WAIT(1000/producerFrameRate);
        }
        
        EXPECT_TRUE_WAIT(receivedFrames_.size() >= framesNum/2, 5000);
        
        {
            for (int i = 0; i < receivedFrames_.size(); i++)
            {
                if (i != receivedFrames_.size()-1)
                    EXPECT_LE(receivedFrames_[i], receivedFrames_[i+1]);
            }
        }
    }

    cout << "Fetched " << receivedFrames_.size() << " frames" << endl;
    
    receiver->stopFetching();
    delete receiver;
    WAIT(params_.freshness*1000);
}
#if 0
TEST_F(NdnReceiverTester, FetchingWithKeyAndDeltaNamespace)
{
    unsigned int jitterSize = 20;
    unsigned int gop = 60;
    unsigned int framesNum = 4*gop;
    unsigned int segmentSize = 100;
    unsigned int bufferSize = gop*2;
    unsigned int producerFrameRate = 30;
    
    params_.bufferSize = bufferSize;
    params_.segmentSize = segmentSize;
    params_.producerRate = producerFrameRate;
    params_.producerId = "testuser4";
    
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params_);
    
    receiver->setLogger(NdnLogger::sharedInstance());
    receiver->setFrameConsumer(this);
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    LOG_TRACE("start fetching");
    EXPECT_EQ(0, receiver->startFetching());
    
    // we should start publishing frames later, so that receiver will get first frame
    // by issuing interest with RightMostChild selector
    WAIT(100);
    string framePrefix;
    EXPECT_EQ(RESULT_OK, MediaSender::getStreamFramePrefix(params_, framePrefix));
    
    // publish more than should - depending on RTT value, fetching will not
    // start right from frame 0
    for (int i = 0; i < 1.2*framesNum; i++)
    {
        frame_->_frameType = (i%gop)?webrtc::kDeltaFrame:webrtc::kKeyFrame;
        frame_->capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
        PacketData::PacketMetadata meta;
        meta.sequencePacketNumber_ = i;
        meta.packetRate_ = producerFrameRate;
        payload = new NdnFrameData(*frame_, meta);
        publishMediaPacket(payload->getLength(), payload->getData(), i,
                           segmentSize, framePrefix, params_.freshness, true);
        WAIT(1000/producerFrameRate);
    }
    EXPECT_TRUE_WAIT(receivedFrames_.size() >= framesNum, 5000);
    cout << "Fetched " << receivedFrames_.size() << " frames" << endl;
    
    {
        int nKeyFramesExpected = framesNum/gop-1; // 1 less key frame as we are feching
                                                  // not from the very first frames
        int nKeyFramesArrived = 0;
        
        for (int i = 0; i < receivedFrames_.size(); i++)
        {
            // check arrival order
            if (i != receivedFrames_.size()-1)
                EXPECT_LE(receivedFrames_[i], receivedFrames_[i+1]);
            

            if (receivedFramesTypes_[i] == webrtc::kKeyFrame)
                nKeyFramesArrived++;
        }
        
        EXPECT_LE(nKeyFramesExpected, nKeyFramesArrived);
    }
    
    receiver->stopFetching();
    delete receiver;
    WAIT(params_.freshness*1000);
}

class VideoReceiverRecorder : public IEncodedFrameConsumer
{
public:
    VideoReceiverRecorder(const char *fileName) :
    writer_(new EncodedFrameWriter(fileName)) {}
    ~VideoReceiverRecorder(){}
            
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        PacketData::PacketMetadata metadata = {0};
        
        if (writer_)
            writer_->writeFrame(encodedImage, metadata);
    }
    
protected:
    EncodedFrameWriter *writer_;
    
};

TEST_F(NdnReceiverTester, Record)
{
    ParamsStruct params = DefaultParams;
    params.producerId = "remap";
    params.ndnHub = "ndn/edu/ucla/cs";
    
    VideoReceiverRecorder recorder("resources/remap30.wei");
    NdnVideoReceiver *receiver = new NdnVideoReceiver(params);
    
    receiver->setFrameConsumer(&recorder);
    receiver->setObserver(this);
    
    EXPECT_EQ(0, receiver->init(ndnReceiverFace_));
    EXPECT_FALSE(obtainedError_);
    
    LOG_TRACE("start fetching");
    EXPECT_EQ(0, receiver->startFetching());
    
    WAIT(30000);
    
    EXPECT_EQ(0, receiver->stopFetching());
}
#endif