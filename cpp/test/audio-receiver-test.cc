//
//  audio-receiver-test.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 10/24/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "audio-receiver.h"
#include "audio-sender.h"
#include "ndnrtc-utils.h"
#include <iostream.h>

using namespace ndnrtc;
using namespace webrtc;

#define USE_RECEIVER_FACE

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

class AudioReceiverTester : public NdnRtcObjectTestHelper,
public IAudioPacketConsumer,
public webrtc::Transport,
public UnitTestHelperNdnNetwork
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        params_ = DefaultParams;
        
        std::string streamAccessPrefix;
        string userPrefix;
        
        ASSERT_EQ(RESULT_OK, MediaSender::getStreamKeyPrefix(params_,
                                                             streamAccessPrefix));
        ASSERT_EQ(RESULT_OK, MediaSender::getUserPrefix(params_, userPrefix));
        
        UnitTestHelperNdnNetwork::NdnSetUp(streamAccessPrefix, userPrefix);
        
        config_.Set<webrtc::AudioCodingModuleFactory>(new webrtc::NewAudioCodingModuleFactory());
        voiceEngine_ = webrtc::VoiceEngine::Create(config_);
        
        ASSERT_TRUE(voiceEngine_ != NULL);
        
        voe_base_ = webrtc::VoEBase::GetInterface(voiceEngine_);
        voe_network_ = webrtc::VoENetwork::GetInterface(voiceEngine_);
        voe_processing_ = webrtc::VoEAudioProcessing::GetInterface(voiceEngine_);
        voe_base_->Init();
        
        sendCS_ = webrtc::CriticalSectionWrapper::CreateCriticalSection();
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        UnitTestHelperNdnNetwork::NdnTearDown();
        
        if (voiceEngine_ != NULL)
        {
            voe_base_->Release();
            voe_network_->Release();
            webrtc::VoiceEngine::Delete(voiceEngine_);
        }
        
        delete sendCS_;
    }
    
    int SendPacket(int channel, const void *data, int len)
    {
        string rtpPrefix;
        NdnAudioData::AudioPacket p = {false, NdnRtcUtils::millisecondTimestamp(),
            (unsigned int)len, (unsigned char*)data};
        NdnAudioData adata(p);
        NdnAudioSender::getStreamFramePrefix(params_, rtpPrefix);
        
//        cout << "publish RTP  " << currentRTPFrame_ << " " << len << endl;
        sendCS_->Enter();
        publishMediaPacket(adata.getLength(), adata.getData(),
                           currentRTPFrame_++, params_.segmentSize,
                           rtpPrefix, params_.freshness);
        sendCS_->Leave();
        
        nSent_++;
        return len;
    }
    
    int SendRTCPPacket(int channel, const void *data, int len)
    {
        string rtcpPrefix;
        NdnAudioData::AudioPacket p = {true, NdnRtcUtils::millisecondTimestamp(),
            (unsigned int)len, (unsigned char*)data};
        NdnAudioData adata(p);
        
        NdnAudioSender::getStreamControlPrefix(params_, rtcpPrefix);
        
//        cout << "publish RTCP " << currentRTPFrame_ << " " << len << endl;
        // using RTP frames counter!!!
        sendCS_->Enter();
#if 0
        NdnAudioSender::getStreamControlPrefix(params_, rtcpPrefix);
        publishMediaPacket(adata.getLength(), adata.getData(),
                           currentRTCPFrame_++, params_.segmentSize,
                           rtcpPrefix, params_.freshness);
#else
        publishMediaPacket(adata.getLength(), adata.getData(),
                           currentRTPFrame_++, params_.segmentSize,
                           rtcpPrefix, params_.freshness);
#endif
        sendCS_->Leave();
        
        nRTCPSent_++;
        
        return len;
    }
    
    void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        
        nReceived_ = 0;
        nSent_ = 0;
        nRTCPReceived_ = 0;
        nRTCPSent_ = 0;
        currentRTPFrame_ = 0;
        currentRTCPFrame_ = 0;
    }
    
    void onInterest(const shared_ptr<const Name>& prefix,
                    const shared_ptr<const Interest>& interest,
                    ndn::Transport& transport)
    {
        LOG_INFO("got interest: %s", interest->getName().toUri().c_str());
    }
    
    void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        FAIL();
    }
    
    void onRTPPacketReceived(unsigned int len, unsigned char *data)
    {
        nReceived_++;
        
        voe_network_->ReceivedRTPPacket(channel_, data, len);
    }
    
    void onRTCPPacketReceived(unsigned int len, unsigned char *data)
    {
        nRTCPReceived_++;
        voe_network_->ReceivedRTCPPacket(channel_, data, len);
    }
    
protected:
    unsigned int nReceived_, nSent_,
                nRTCPSent_, nRTCPReceived_,
                currentRTPFrame_, currentRTCPFrame_;
    
    int channel_;
    webrtc::VoiceEngine *voiceEngine_;
    webrtc::VoEBase *voe_base_;
    webrtc::VoENetwork *voe_network_;
    webrtc::Config config_;
    webrtc::VoEAudioProcessing *voe_processing_;
    webrtc::CriticalSectionWrapper *sendCS_;
};

TEST_F(AudioReceiverTester, TestFetch)
{
    unsigned int expectedFetchedPacketsNum = 100;
    unsigned int publishPacketsNum = 1.5*100;
    params_.streamName = "audio-receiver-test";
    params_.streamThread = "pcmu2";
    
    NdnAudioReceiver receiver(params_);
    
    receiver.setFrameConsumer(this);
    
    EXPECT_EQ(RESULT_OK, receiver.init(ndnReceiverFace_));
    EXPECT_EQ(RESULT_OK, receiver.startFetching());
    
    channel_ = voe_base_->CreateChannel();
    
    voe_processing_->SetEcStatus(true);
    
    ASSERT_LE(0, channel_);
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *this));
    
    EXPECT_EQ(0, voe_base_->StartReceive(channel_));
    EXPECT_EQ(0, voe_base_->StartSend(channel_));
    EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
    
    EXPECT_TRUE_WAIT(nSent_ + nRTCPSent_ >= publishPacketsNum, publishPacketsNum*20);
    
    EXPECT_EQ(0, voe_base_->StopSend(channel_));
    EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
    EXPECT_EQ(0, voe_base_->StopReceive(channel_));
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));

    EXPECT_TRUE_WAIT(nReceived_+nRTCPReceived_ >= expectedFetchedPacketsNum, 2*publishPacketsNum*20);
    
    cout << "Published: RTP (" << nSent_ << "), RTCP (" << nRTCPSent_ << ") "<< endl;
    cout << "Fetched:   RTP (" << nReceived_ << "), RTCP (" << nRTCPReceived_ << ") "<< endl;
    
    EXPECT_EQ(RESULT_OK, receiver.stopFetching());
    WAIT(params_.freshness*1000);
}