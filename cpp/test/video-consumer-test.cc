//
//  consumer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "video-consumer.h"
#include "rtt-estimation.h"
#include "sender-channel.h"
#include "buffer-estimator.h"
#include "consumer-channel.h"
#include "pipeliner.h"

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class ConsumerTests : public UnitTestHelperNdnNetwork,
public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        params_ = DefaultParams;
        params_.captureDeviceId = 1;
        params_.interestTimeout = 2000;
    }
    
    void TearDown()
    {
    }
    
protected:
    shared_ptr<FaceProcessor> faceProcessor_;
    shared_ptr<InterestQueue> interestQueue_;
    shared_ptr<RttEstimation> rttEstimation_;
    shared_ptr<NdnSenderChannel> sendChannel_;
    
    void
    startPublishing()
    {
        sendChannel_.reset(new NdnSenderChannel(params_, DefaultParamsAudio));
        sendChannel_->setLogger(&Logger::sharedInstance());
        
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->init()));
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->startTransmission()));
    }
    
    void
    stopPublishing()
    {
        EXPECT_EQ(RESULT_OK, sendChannel_->stopTransmission());
    }
};
#if 1
TEST_F(ConsumerTests, TestFetching)
{
    params_.useFec = false;
    params_.captureDeviceId = 1;
    
    faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
    faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                              bind(&UnitTestHelperNdnNetwork::onInterest,
                                                   this, _1, _2, _3),
                                              bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                   this, _1));
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
    interestQueue_->setDescription("cchannel-iqueue");
    rttEstimation_.reset(new RttEstimation());
    
    int chasingDeadline = 5000;
    
    shared_ptr<VideoConsumer> vconsumer(new VideoConsumer(params_, interestQueue_));
    vconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
    
    ASSERT_EQ(RESULT_OK, vconsumer->init());
    EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
    
    startPublishing();
    faceProcessor_->startProcessing();
    EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
    
    EXPECT_EQ(RESULT_OK,vconsumer->start());
    EXPECT_EQ(Consumer::StateChasing, vconsumer->getState());
    
    {
        int64_t t = NdnRtcUtils::millisecondTimestamp();
        EXPECT_TRUE_WAIT(Consumer::StateFetching == vconsumer->getState(), chasingDeadline);
        std::cout << "chasing time " << NdnRtcUtils::millisecondTimestamp()-t << endl;
    }
        
    EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
    
    WAIT(5000);
    
    int64_t t = NdnRtcUtils::millisecondTimestamp();
    EXPECT_EQ(RESULT_OK,vconsumer->stop());
    EXPECT_GT(5000, (NdnRtcUtils::millisecondTimestamp()-t));
    EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
    
    faceProcessor_->stopProcessing();
    stopPublishing();
    WAIT(params_.freshness*1000);
}
#endif
#if 1
TEST_F(ConsumerTests, TestDelayedProducer)
{
    {
        params_.useFec = false;
        params_.captureDeviceId = 1;
        
        faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
        faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                                  bind(&UnitTestHelperNdnNetwork::onInterest,
                                                       this, _1, _2, _3),
                                                  bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                       this, _1));
        
        interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
        interestQueue_->setDescription("cchannel-iqueue");
        rttEstimation_.reset(new RttEstimation());
        
        shared_ptr<VideoConsumer> vconsumer(new VideoConsumer(params_, interestQueue_));
        vconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
        
        ASSERT_EQ(RESULT_OK, vconsumer->init());
        
        faceProcessor_->startProcessing();
        
        EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
        EXPECT_EQ(RESULT_OK,vconsumer->start());
        WAIT(500);
        
        EXPECT_EQ(Consumer::StateChasing, vconsumer->getState());
        WAIT(3000);
        
        // now start publishing
        startPublishing();
        WAIT(3000); // allow some time for chasing
        EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
        WAIT(3000);
        EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
        
        EXPECT_EQ(RESULT_OK,vconsumer->stop());
        EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
        
        faceProcessor_->stopProcessing();
        stopPublishing();
    }
    WAIT(params_.freshness*1000);
}
#endif
#if 1
TEST_F(ConsumerTests, TestRecovery)
{
    params_.useFec = false;
    params_.captureDeviceId = 1;
    
    faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
    faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                              bind(&UnitTestHelperNdnNetwork::onInterest,
                                                   this, _1, _2, _3),
                                              bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                   this, _1));
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
    interestQueue_->setDescription("cchannel-iqueue");
    rttEstimation_.reset(new RttEstimation());
    
    shared_ptr<VideoConsumer> vconsumer(new VideoConsumer(params_, interestQueue_));
    vconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
    
    ASSERT_EQ(RESULT_OK, vconsumer->init());
    
    faceProcessor_->startProcessing();
    startPublishing();
    
    int chasingInterval = 5000;
    int recoveryInterval = 2*Pipeliner::MaxInterruptionDelay;
    int playbackInterval = 5000;
    
    EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
    EXPECT_EQ(RESULT_OK,vconsumer->start());
    EXPECT_EQ(Consumer::StateChasing, vconsumer->getState());
    WAIT(chasingInterval); // allow some time for chasing
    
    EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
    WAIT(playbackInterval);
    
    stopPublishing();
    WAIT(recoveryInterval);
    EXPECT_EQ(Consumer::StateChasing, vconsumer->getState());
    
    startPublishing();
    WAIT(chasingInterval);

    EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
    WAIT(playbackInterval);
    
    EXPECT_EQ(Consumer::StateFetching, vconsumer->getState());
    EXPECT_EQ(RESULT_OK,vconsumer->stop());
    EXPECT_EQ(Consumer::StateInactive, vconsumer->getState());
    
    faceProcessor_->stopProcessing();
    stopPublishing();
    WAIT(params_.freshness*1000);
}
#endif