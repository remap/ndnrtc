//
//  pipeliner-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 3/3/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "pipeliner.h"
#include "ndnrtc-utils.h"
#include "ndnrtc-namespace.h"
#include "sender-channel.h"
#include "renderer.h"
#include "video-decoder.h"
#include "video-playout.h"

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class PipelinerConsumerMock : public ConsumerMock
{
public:
    PipelinerConsumerMock(string logfile):ConsumerMock(logfile){}
    
    void
    setFrameBuffer(const shared_ptr<ndnrtc::new_api::FrameBuffer>& frameBuffer)
    { frameBuffer_ = frameBuffer; }
    
    void
    setBufferEstimator(const shared_ptr<BufferEstimator>& bufferEstimator)
    { bufferEstimator_= bufferEstimator; }
    
    void
    setChaseEstimation(const shared_ptr<ChaseEstimation>& chaseEstimation)
    { chaseEstimation_ = chaseEstimation; }
    
    void
    setAssembler(const shared_ptr<IPacketAssembler>& ndnAssembler)
    { packetAssembler_ = ndnAssembler; }
    
    void
    setInterestQueue(const shared_ptr<InterestQueue>& interestQueue)
    { interestQueue_ = interestQueue; }
};

class PipelinerTests : public UnitTestHelperNdnNetwork,
public NdnRtcObjectTestHelper,
public IPacketAssembler
{
public:
    void SetUp()
    {
        params_ = DefaultParams;
        params_.captureDeviceId = 2;
        
        shared_ptr<string> accessPref = NdnRtcNamespace::getStreamKeyPrefix(params_);
        shared_ptr<string> userPref = NdnRtcNamespace::getStreamFramePrefix(params_);
        UnitTestHelperNdnNetwork::NdnSetUp(*accessPref, *userPref);
        
        PipelinerConsumerMock *mock = new PipelinerConsumerMock("pipeliner.log");//ENV_LOGFILE);
        mock->setParameters(params_);
        consumer_.reset(mock);
        
        frameBuffer_.reset(new ndnrtc::new_api::FrameBuffer(consumer_));
        interestQueue_.reset(new InterestQueue(consumer_, ndnFace_));
        
        bufferEstimator_.reset(new BufferEstimator());
        bufferEstimator_->setMinimalBufferSize(params_.jitterSize);
        
        chaseEstimation_.reset(new ChaseEstimation());
        packetAssembler_.reset(this);
        
        mock->setAssembler(packetAssembler_);
        mock->setFrameBuffer(frameBuffer_);
        mock->setBufferEstimator(bufferEstimator_);
        mock->setChaseEstimation(chaseEstimation_);
        mock->setInterestQueue(interestQueue_);
    }
    
    void TearDown()
    {
        UnitTestHelperNdnNetwork::NdnTearDown();
    }
    
    void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data)
    {
        UnitTestHelperNdnNetwork::onData(interest, data);
        frameBuffer_->newData(*data);
    }
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        cout << "timeout" << endl;
        
        frameBuffer_->interestTimeout(*interest);
    }
    
    ndn::OnData getOnDataHandler()
    {
        return bind(&PipelinerTests::onData, this, _1, _2);
    }
    ndn::OnTimeout getOnTimeoutHandler()
    {
        return bind(&PipelinerTests::onTimeout, this, _1);
    }
    
protected:
    shared_ptr<const Consumer> consumer_;
    shared_ptr<ndnrtc::new_api::FrameBuffer> frameBuffer_;
    shared_ptr<BufferEstimator> bufferEstimator_;
    shared_ptr<ChaseEstimation> chaseEstimation_;
    shared_ptr<InterestQueue> interestQueue_;
    shared_ptr<IPacketAssembler> packetAssembler_;
    shared_ptr<NdnSenderChannel> sendChannel_;
    
    void
    startPublishing()
    {
        sendChannel_.reset(new NdnSenderChannel(params_, DefaultParamsAudio));
        
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->init()));
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->startTransmission()));
    }
    
    void
    stopPublishing()
    {
        EXPECT_EQ(RESULT_OK, sendChannel_->stopTransmission());
    }
};
#if 0
TEST_F(PipelinerTests, TestCreateDelete)
{
    Pipeliner p(consumer_);
}

TEST_F(PipelinerTests, TestInit)
{
    startPublishing();
    startProcessingNdn();
    
    Pipeliner p(consumer_);
    p.start();
    
    frameBuffer_->init();
    EXPECT_TRUE_WAIT(ndnrtc::new_api::FrameBuffer::State::Valid ==
                     frameBuffer_->getState(), 10000);
    
    p.stop();

    stopProcessingNdn();
    stopPublishing();
}
#endif
TEST_F(PipelinerTests, TestFetching)
{
    VideoPlayout playout(consumer_);
    
    NdnRenderer renderer(0, params_);
    renderer.init();

    NdnVideoDecoder decoder(params_);
    decoder.init();
    decoder.setFrameConsumer(&renderer);
    
    playout.init(&decoder);
    
    startPublishing();
    startProcessingNdn();
    
    frameBuffer_->init();
    
    Pipeliner p(consumer_);
    p.start();
    
    EXPECT_TRUE_WAIT(ndnrtc::new_api::FrameBuffer::State::Valid ==
                     frameBuffer_->getState(), 100000);
    
    PacketData* data = nullptr;
    
    renderer.startRendering();
    
    playout.start();
    WAIT(100000);
    playout.stop();
    
//    int time = 100000;
//    while (time > 0)
//    {
//        int playbackDuration;
//        
//        if (frameBuffer_->getState() == ndnrtc::new_api::FrameBuffer::Valid)
//        {
//            webrtc::EncodedImage frame;
//            if (data)
//            {
//                delete data;
//                data = nullptr;
//            }
//            
//            frameBuffer_->acquireSlot(&data);
//            
//            if (data)
//            {
//                EXPECT_EQ(RESULT_OK, ((NdnFrameData*)data)->getFrame(frame));
//                
//                LogTrace(consumer_->getLogFile())
//                << "push for decode " << ((frame._frameType == webrtc::kKeyFrame)?"KEY":"DELTA") << endl;
//                
//                decoder.onEncodedFrameDelivered(frame);
//            }
//            else
//            {
//                LogTrace(consumer_->getLogFile())
//                << "no data for decode" << endl;
//            }
//
//            playbackDuration = frameBuffer_->releaseAcquiredSlot();
//        }
//        
//        if (playbackDuration <= 0)
//            playbackDuration = 30;
//        
//        time -= playbackDuration;
//        
//        usleep(playbackDuration*1000);
//    }
    
    p.stop();
    
    renderer.stopRendering();
    
    stopProcessingNdn();
    stopPublishing();
    
}
