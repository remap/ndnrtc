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

#include "test-common.h"
#include "audio-channel.h"

using namespace ndnrtc;
using namespace std;
using namespace webrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

class NdnAudioChannelTester : public NdnRtcObjectTestHelper,
public UnitTestHelperNdnNetwork,
public webrtc::Transport,
public IAudioPacketConsumer
{
public:
    void SetUp()
    {
        setupWebRTCLogging();
        
        params_ = DefaultParams;
        params_.streamName = "audio0";
        params_.streamThread = "pcmu2";
        params_.freshness = 5;
        
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
        
        sendCS_ = webrtc::CriticalSectionWrapper::CreateCriticalSection();
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
        
        delete sendCS_;        
    }
    
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);

        INFO("got timeout for %s", interest->getName().toUri().c_str());
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
    
    int SendPacket(int channel, const void *data, int len)
    {
        string rtpPrefix;
        NdnAudioData::AudioPacket p = {false, (unsigned int)len, (unsigned char*)data};
        NdnAudioData adata(p);
        NdnAudioSender::getStreamFramePrefix(params_, rtpPrefix);
        
        sendCS_->Enter();
        publishMediaPacket(adata.getLength(), adata.getData(),
                           currentRTPFrame_++, params_.segmentSize,
                           rtpPrefix, params_.freshness);
        sendCS_->Leave();
        
        nRTPSent_++;
        return len;
    }
    
    int SendRTCPPacket(int channel, const void *data, int len)
    {
        string rtcpPrefix;
        NdnAudioData::AudioPacket p = {true, (unsigned int)len, (unsigned char*)data};
        NdnAudioData adata(p);
        
        NdnAudioSender::getStreamControlPrefix(params_, rtcpPrefix);
        
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
    
    void onRTPPacketReceived(unsigned int len, unsigned char *data)
    {
        rtpDataFetched_++;
    }
    
    void onRTCPPacketReceived(unsigned int len, unsigned char *data)
    {
        rtcpDataFetched_++;
    }
    
    void onErrorOccurred(const char *errorMessage)
    {
        NdnRtcObjectTestHelper::onErrorOccurred(errorMessage);
        
        INFO("ERROR occurred: %s", errorMessage);
    }
    
protected:
    unsigned int nRTPSent_ = 0, nRTCPSent_ = 0,
                    currentRTPFrame_ = 0, currentRTCPFrame_ = 0;
    unsigned int rtpDataFetched_ = 0, rtcpDataFetched_ = 0;
    
    VoiceEngine *voiceEngine_ = NULL;
    VoEBase *voe_base_ = NULL;
    Config config_;
    webrtc::CriticalSectionWrapper *sendCS_;
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
    // based on the observations: 50 packets ~ 1 second of recording
    int nPackets = 100;
    int overhead = 500;
    int64_t publishTime = nPackets*20+overhead;
 
    // make sure packets will not be dropped
    params_.freshness = (publishTime*2/1000 < 1.)? 1 : publishTime*2/1000;
    
    NdnAudioSendChannel sendChannel(voiceEngine_);
    sendChannel.setObserver(this);
    
    EXPECT_EQ(0, sendChannel.init(params_, ndnReceiverTransport_));
    
    UnitTestHelperNdnNetwork::startProcessingNdn();

    int64_t startTime = millisecondTimestamp();
    EXPECT_EQ(RESULT_OK,sendChannel.start());
    EXPECT_FALSE(obtainedError_);
    
    // now check what we have on the network
    string rtpPrefix, rtcpPrefix;
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamFramePrefix(params_,
                                                              rtpPrefix));
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamControlPrefix(params_,
                                                           rtcpPrefix));
    Name rtpPacketPrefix(rtpPrefix);
    Name rtcpPacketPrefix(rtcpPrefix);
    
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
        WAIT(10);
        publishTime -= 10;
        
        if (publishTime <= 0)
        {
            publishTime = 0;
            EXPECT_EQ(0,sendChannel.stop());
        }
    }

    if (publishTime > 0)
    {
        WAIT(publishTime);
        EXPECT_EQ(RESULT_OK,sendChannel.stop());
    }
    
    // make some oberhead for waiting data to be fetched
    int fetchTime = publishTime;
    
    EXPECT_TRUE_WAIT(((rtpDataFetched_ + rtcpDataFetched_) == nPackets),
                     fetchTime);
    
    UnitTestHelperNdnNetwork::stopProcessingNdn();
    
    EXPECT_EQ(0,nReceivedTimeout_);
    
    // expect at least 80% RTP and 1% RTCP
    cout << "RTP fetched: " << rtpDataFetched_ << endl;
    cout << "RTCP fetched: " << rtcpDataFetched_ << endl;
    
    EXPECT_LE(0.80*nPackets, rtpDataFetched_);
    EXPECT_LE(0.01*nPackets, rtcpDataFetched_);
    EXPECT_LE(nPackets, rtpDataFetched_+rtcpDataFetched_);
    
    // wait unless data will become stale
    WAIT(params_.freshness*1000);
}

class AudioReceiverChannelTester : public NdnAudioReceiveChannel
{
public:
    AudioReceiverChannelTester(webrtc::VoiceEngine *ve):
        NdnAudioReceiveChannel(ve){}
    
    unsigned int nRTPReceived_ = 0, nRTCPReceived_ = 0;
    
    void onRTPPacketReceived(unsigned int len, unsigned char *data)
    {
        NdnAudioReceiveChannel::onRTPPacketReceived(len, data);
        nRTPReceived_++;
    }
    void onRTCPPacketReceived(unsigned int len, unsigned char *data)
    {
        NdnAudioReceiveChannel::onRTCPPacketReceived(len, data);
        nRTCPReceived_++;
    }
};

TEST_F(NdnAudioChannelTester, TestReceiveChannel)
{
    params_.bufferSize = 5;
    params_.slotSize = 1500;
    
    AudioReceiverChannelTester receiverTester(voiceEngine_);
    
    receiverTester.setObserver(this);
    EXPECT_EQ(RESULT_OK, receiverTester.init(params_, ndnReceiverFace_));
    
    int channel = voe_base_->CreateChannel();
    
    ASSERT_LE(0,channel);
    
    VoENetwork *voe_network = VoENetwork::GetInterface(voiceEngine_);
    EXPECT_EQ(0, voe_network->RegisterExternalTransport(channel, *this));
    
    EXPECT_EQ(RESULT_OK, receiverTester.start());
    EXPECT_EQ(0, voe_base_->StartSend(channel));
    
    uint64_t overhead = 200;
    unsigned int publishPacketsNum = 500;
    EXPECT_TRUE_WAIT((nRTPSent_+nRTCPSent_) >= publishPacketsNum,
                     publishPacketsNum*20+overhead);
    
    sendCS_->Enter();
    { // stop publishing
        int res = voe_base_->StopSend(channel);
        
        EXPECT_EQ(0, res);
        
        if (res < 0)
            INFO("error while stopping voe_base: %d", voe_base_->LastError());
        
        EXPECT_EQ(0, voe_network->DeRegisterExternalTransport(channel));
        voe_network->Release();
    }
    sendCS_->Leave();

    EXPECT_TRUE_WAIT((receiverTester.nRTPReceived_+receiverTester.nRTCPReceived_) >= publishPacketsNum, publishPacketsNum*20);
    
    { // stop fetching
        EXPECT_EQ(RESULT_OK, receiverTester.stop());
        EXPECT_GE(nRTPSent_, receiverTester.nRTPReceived_);
        EXPECT_GE(nRTCPSent_, receiverTester.nRTCPReceived_);
    }
    WAIT(params_.freshness*1000);
}

TEST_F(NdnAudioChannelTester, TestSendReceive)
{
    int nPackets = 500;
    int overhead = 500;
    int recordingTime = nPackets*20+overhead;
    
    params_.freshness = (recordingTime/1000.)<1.?1.:recordingTime/1000.;
    
    { // send datda
        NdnAudioReceiveChannel recevier(voiceEngine_);
        NdnAudioSendChannel sender(voiceEngine_);
        
        EXPECT_EQ(RESULT_OK, recevier.init(params_, ndnReceiverFace_));
        EXPECT_EQ(RESULT_OK, sender.init(params_, ndnTransport_));
        
        EXPECT_EQ(RESULT_OK, recevier.start());
        EXPECT_EQ(RESULT_OK, sender.start());
        
        WAIT(recordingTime);
        
        EXPECT_EQ(RESULT_OK, recevier.stop());
        EXPECT_EQ(RESULT_OK, sender.stop());
    }
    WAIT(params_.freshness*1000);
}
