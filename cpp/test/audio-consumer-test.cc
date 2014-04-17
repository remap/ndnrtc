//
//  audio-consumer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "audio-consumer.h"
#include "rtt-estimation.h"
#include "sender-channel.h"
#include "buffer-estimator.h"
#include "pipeliner.h"

::testing
::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class AudioConsumerTests : public UnitTestHelperNdnNetwork,
public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        params_ = DefaultParamsAudio;
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
        sendChannel_.reset(new NdnSenderChannel(DefaultParams, params_));
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

TEST_F(AudioConsumerTests, TestFetching)
{
    ParamsStruct params = DefaultParamsAudio;
    
    faceProcessor_ = FaceProcessor::createFaceProcessor(params);
    faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params),
                                              bind(&UnitTestHelperNdnNetwork::onInterest,
                                                   this, _1, _2, _3),
                                              bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                   this, _1));
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
    interestQueue_->setDescription("cchannel-iqueue");
    rttEstimation_.reset(new RttEstimation());
    
    int chasingDeadline = 5000;
    
    shared_ptr<AudioConsumer> aconsumer(new AudioConsumer(params_, interestQueue_));
    aconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
    
    ASSERT_EQ(RESULT_OK, aconsumer->init());
    EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
    
    startPublishing();
    faceProcessor_->startProcessing();
    EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
    
    EXPECT_EQ(RESULT_OK,aconsumer->start());
    EXPECT_EQ(Consumer::StateChasing, aconsumer->getState());
    
    {
        int64_t t = NdnRtcUtils::millisecondTimestamp();
        EXPECT_TRUE_WAIT(Consumer::StateFetching == aconsumer->getState(), chasingDeadline);
        std::cout << "chasing time " << NdnRtcUtils::millisecondTimestamp()-t << endl;
    }
    
    EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
    
    WAIT(5000);
    
    int64_t t = NdnRtcUtils::millisecondTimestamp();
    EXPECT_EQ(RESULT_OK,aconsumer->stop());
    EXPECT_GT(5000, (NdnRtcUtils::millisecondTimestamp()-t));
    EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
    
    faceProcessor_->stopProcessing();
    stopPublishing();
    WAIT(params_.freshness*1000);
}
#if 1
TEST_F(AudioConsumerTests, TestDelayedProducer)
{
    {
        params_.useFec = false;
        
        faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
        faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                                  bind(&UnitTestHelperNdnNetwork::onInterest,
                                                       this, _1, _2, _3),
                                                  bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                       this, _1));
        
        interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
        interestQueue_->setDescription("cchannel-iqueue");
        rttEstimation_.reset(new RttEstimation());
        
        shared_ptr<AudioConsumer> aconsumer(new AudioConsumer(params_, interestQueue_));
        aconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
        
        ASSERT_EQ(RESULT_OK, aconsumer->init());
        
        faceProcessor_->startProcessing();
        
        EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
        EXPECT_EQ(RESULT_OK,aconsumer->start());
        WAIT(500);
        
        EXPECT_EQ(Consumer::StateChasing, aconsumer->getState());
        WAIT(3000);
        
        // now start publishing
        startPublishing();
        WAIT(3000); // allow some time for chasing
        EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
        WAIT(3000);
        EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
        
        EXPECT_EQ(RESULT_OK,aconsumer->stop());
        EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
        
        faceProcessor_->stopProcessing();
        stopPublishing();
    }
    WAIT(params_.freshness*1000);
}
#endif
#if 1
TEST_F(AudioConsumerTests, TestRecovery)
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
    
    shared_ptr<AudioConsumer> aconsumer(new AudioConsumer(params_, interestQueue_));
    aconsumer->setLogger(&ndnlog::new_api::Logger::sharedInstance());
    
    ASSERT_EQ(RESULT_OK, aconsumer->init());
    
    faceProcessor_->startProcessing();
    startPublishing();
    
    int chasingInterval = 5000;
    int recoveryInterval = 2*Pipeliner::MaxInterruptionDelay;
    int playbackInterval = 5000;
    
    EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
    EXPECT_EQ(RESULT_OK,aconsumer->start());
    EXPECT_EQ(Consumer::StateChasing, aconsumer->getState());
    WAIT(chasingInterval); // allow some time for chasing
    
    EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
    WAIT(playbackInterval);
    
    stopPublishing();
    WAIT(recoveryInterval);
    EXPECT_EQ(Consumer::StateChasing, aconsumer->getState());
    
    startPublishing();
    WAIT(chasingInterval);
    
    EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
    WAIT(playbackInterval);
    
    EXPECT_EQ(Consumer::StateFetching, aconsumer->getState());
    EXPECT_EQ(RESULT_OK,aconsumer->stop());
    EXPECT_EQ(Consumer::StateInactive, aconsumer->getState());
    
    faceProcessor_->stopProcessing();
    stopPublishing();
    WAIT(params_.freshness*1000);
}
#endif