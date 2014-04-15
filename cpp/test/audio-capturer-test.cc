//
//  audio-capturer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "audio-capturer.h"
#include "ndnrtc-utils.h"

::testing
::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class AudioCapturerTests : public NdnRtcObjectTestHelper,
public IAudioFrameConsumer
{
public:
    void SetUp()
    {
        nRtpDelivered_ = 0;
        nRtcpDelivered_ = 0;
    }
    
    void TearDown()
    {
        
    }
    
protected:
    int nRtpDelivered_, nRtcpDelivered_;
    
    void onDeliverRtpFrame(unsigned int len, unsigned char *data)
    {
        nRtpDelivered_++;
    }
    
    void onDeliverRtcpFrame(unsigned int len, unsigned char *data)
    {
        nRtcpDelivered_++;
    }
    
};

TEST_F(AudioCapturerTests, TestCreate)
{
    AudioCapturer *capturer = new AudioCapturer(DefaultParamsAudio,
                                                NdnRtcUtils::sharedVoiceEngine());
    delete capturer;
}
TEST_F(AudioCapturerTests, TestInit)
{
    AudioCapturer *capturer = new AudioCapturer(DefaultParamsAudio,
                                                NdnRtcUtils::sharedVoiceEngine());
    
    EXPECT_EQ(RESULT_OK, capturer->init());
    EXPECT_FALSE(obtainedError_);
    
    delete capturer;
}


TEST_F(AudioCapturerTests, TestCapture)
{
    AudioCapturer *capturer = new AudioCapturer(DefaultParamsAudio,
                                                NdnRtcUtils::sharedVoiceEngine());
    double delta = 0.05;
    double avgPacketRate = 50;
    double duration = 2000;
    
    capturer->init();
    capturer->setFrameConsumer(this);
    EXPECT_EQ(RESULT_OK, capturer->startCapture());
    WAIT(duration);
    EXPECT_EQ(RESULT_OK, capturer->stopCapture());
    
    std::cout << "RTP received " << nRtpDelivered_ << std::endl;
    std::cout << "RTCP received " << nRtcpDelivered_ << std::endl;
    
    EXPECT_TRUE((nRtpDelivered_+nRtcpDelivered_) >= (duration/1000.*avgPacketRate)*(1-delta));
    
    
    delete capturer;
}