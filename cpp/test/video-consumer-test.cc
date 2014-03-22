//
//  consumer-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 3/20/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "video-consumer.h"
#include "rtt-estimation.h"
#include "sender-channel.h"
#include "buffer-estimator.h"
#include "consumer-channel.h"

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
TEST_F(ConsumerTests, Test)
{
    faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
    faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                              bind(&UnitTestHelperNdnNetwork::onInterest,
                                                   this, _1, _2, _3),
                                              bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                                   this, _1));
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
    rttEstimation_.reset(new RttEstimation());
    
    shared_ptr<VideoConsumer> vconsumer(new VideoConsumer(params_, interestQueue_));
    
    EXPECT_EQ(RESULT_OK, vconsumer->init());
    
    startPublishing();
    faceProcessor_->startProcessing();
    
    EXPECT_EQ(RESULT_OK,vconsumer->start());
    WAIT(100000);
    EXPECT_EQ(RESULT_OK,vconsumer->stop());
    
    faceProcessor_->stopProcessing();
    stopPublishing();
    
}
#endif
TEST_F(ConsumerTests, Test2)
{
    ConsumerChannel cchannel(DefaultParams, DefaultParamsAudio);
    startPublishing();
    
    EXPECT_EQ(RESULT_OK, cchannel.init());
    EXPECT_EQ(RESULT_OK, cchannel.startTransmission());
    
    WAIT(100000);
    
    EXPECT_EQ(RESULT_OK, cchannel.stopTransmission());
    stopPublishing();
}
