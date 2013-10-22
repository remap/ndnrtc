//
//  ndnaudio-transport.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 10/21/13
//

#undef NDN_LOGGING
#define DEBUG
#undef NDN_DETAILED

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_TRACE

#include "test-common.h"
#include "ndnaudio-transport.h"

using namespace ndnrtc;
using namespace std;
using namespace webrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

#if 0
TEST(NdnAudioTransportTest, CreateDelete)
{
    ASSERT_NO_THROW(
                    NdnAudioTransport *audioTransport = new NdnAudioTransport();
                    delete audioTransport;
                    );
}
#endif

class NdnAudioTransportTester : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        setupWebRTCLogging();
        
        NdnRtcObjectTestHelper::SetUp();
        config_.Set<AudioCodingModuleFactory>(new NewAudioCodingModuleFactory());
        voiceEngine_ = VoiceEngine::Create(config_);
        
        ASSERT_TRUE(voiceEngine_ != NULL);
        
        voe_base_ = VoEBase::GetInterface(voiceEngine_);
        voe_network_ = VoENetwork::GetInterface(voiceEngine_);
        voe_base_->Init();
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (voiceEngine_ != NULL)
        {
            voe_base_->Release();
            voe_network_->Release();
            VoiceEngine::Delete(voiceEngine_);
        }
    }
    
protected:
    int channel_;
    VoiceEngine *voiceEngine_;
    VoEBase *voe_base_;
    VoENetwork *voe_network_;
    Config config_;
};

TEST_F(NdnAudioTransportTester, TestAsExternalTransport)
{
    NdnAudioTransport audioTransport;
    
    EXPECT_EQ(0, audioTransport.init(voe_network_));
    
    channel_ = voe_base_->CreateChannel();
    
    TRACE("channel id %d", channel_);
    
    ASSERT_LE(0, channel_);
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, audioTransport));
    
    EXPECT_EQ(0, voe_base_->StartReceive(channel_));
    EXPECT_EQ(0, voe_base_->StartSend(channel_));
    EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
    
    WAIT(5000);
    
    EXPECT_EQ(0, voe_base_->StopSend(channel_));
    EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
    EXPECT_EQ(0, voe_base_->StopReceive(channel_));
    
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
}
