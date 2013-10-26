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
#include "audio-channel.h"

using namespace ndnrtc;
using namespace std;
using namespace webrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

class NdnAudioChannelTester : public NdnRtcObjectTestHelper,
public UnitTestHelperNdnNetwork
{
public:
    void SetUp()
    {
        setupWebRTCLogging();
        
        params_ = DefaultParams;
        
        NdnRtcObjectTestHelper::SetUp();
        
        std::string streamAccessPrefix, userPrefix;
        ASSERT_EQ(RESULT_OK, MediaSender::getStreamKeyPrefix(params_,
                                                             streamAccessPrefix));
        ASSERT_EQ(RESULT_OK, MediaSender::getUserPrefix(params_, userPrefix));
        
        UnitTestHelperNdnNetwork::NdnSetUp(streamAccessPrefix, userPrefix);
        
        config_.Set<AudioCodingModuleFactory>(new NewAudioCodingModuleFactory());
        voiceEngine_ = VoiceEngine::Create(config_);
        
        ASSERT_TRUE(voiceEngine_ != NULL);
        
        VoEAudioProcessing *voe_proc = VoEAudioProcessing::GetInterface(voiceEngine_);
        
        voe_proc->SetEcStatus(true);
        voe_proc->Release();
        
        voe_base_ = VoEBase::GetInterface(voiceEngine_);
        voe_base_->Init();
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        UnitTestHelperNdnNetwork::NdnTearDown();
        
        if (voiceEngine_ != NULL)
        {
            voe_base_->Release();
            VoiceEngine::Delete(voiceEngine_);
        }
    }
    
    void onData(const shared_ptr<const Interest>& interest,
                const shared_ptr<Data>& data)
    {
        UnitTestHelperNdnNetwork::onData(interest, data);
        
        unsigned char *rawAudioData = (unsigned char*)data->getContent().buf();
        
        NdnAudioData::AudioPacket packet;
        
        EXPECT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data->getContent().size(),
                                                       rawAudioData, packet));
        
        if (packet.isRTCP_)
            rtcpDataFetched_++;
        else
            rtpDataFetched_++;
    }
    
protected:
    unsigned int rtpDataFetched_ = 0, rtcpDataFetched_ = 0;
    int channel_;
    VoiceEngine *voiceEngine_ = NULL;
    VoEBase *voe_base_ = NULL;
    Config config_;
};

TEST_F(NdnAudioChannelTester, TestSendChannelCreateDelete)
{
    ASSERT_NO_THROW(
                    NdnAudioSendChannel *ch = new NdnAudioSendChannel(voiceEngine_);
                    delete ch;
    );
}


TEST_F(NdnAudioChannelTester, TestSendChannel)
{
    NdnAudioSendChannel sendChannel(voiceEngine_);
    sendChannel.setObserver(this);
    
    params_.freshness = 10;
    EXPECT_EQ(0, sendChannel.init(params_, ndnTransport_));
    
    UnitTestHelperNdnNetwork::startProcessingNdn();

    EXPECT_EQ(RESULT_OK,sendChannel.start());
    EXPECT_FALSE(obtainedError_);
    WAIT(2000); // wait a bit
    EXPECT_EQ(RESULT_OK,sendChannel.stop());
    
    // now check what we have on the network
    string rtpPrefix, rtcpPrefix;
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamFramePrefix(params_,
                                                              rtpPrefix));
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamControlPrefix(params_,
                                                           rtcpPrefix));
    Name rtpPacketPrefix(rtpPrefix);
    Name rtcpPacketPrefix(rtcpPrefix);
    
    // should be 100 packets for sure
    int nPackets = 100;
    for (int i = 0; i < nPackets; i++)
    {
        Name prefix = rtpPacketPrefix;
        
        char frameNoStr[3];
        memset(&frameNoStr[0], 0, 3);
        sprintf(&frameNoStr[0], "%d", i);
        
        prefix.addComponent((const unsigned char*)&frameNoStr[0], strlen(frameNoStr));
        
        INFO("expressing %s", prefix.toUri().c_str());
        ndnFace_->expressInterest(prefix, bind(&NdnAudioChannelTester::onData, this, _1, _2),
                                  bind(&NdnAudioChannelTester::onTimeout, this, _1));
    }
    
    EXPECT_TRUE_WAIT(rtpDataFetched_ + rtcpDataFetched_ == nPackets, 5000);
    
    UnitTestHelperNdnNetwork::stopProcessingNdn();
    
    EXPECT_GT(0, rtpDataFetched_);
    EXPECT_GT(0, rtcpDataFetched_);
    EXPECT_LE(nPackets, rtpDataFetched_+rtcpDataFetched_);
}

