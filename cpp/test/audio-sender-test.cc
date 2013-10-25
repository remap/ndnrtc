//
//  audio-sender-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR

#define NDN_DETAILED
#define NDN_TRACE
#define NDN_DEBUG

#include "test-common.h"
#include "audio-sender.h"
#include "frame-buffer.h"
#include <string.h>

using namespace ndnrtc;
using namespace std;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(AudioSenderParamsTest, CheckPrefixes)
{
    ParamsStruct p = DefaultParams;
    
    p.streamName = "audio0";
    p.streamThread = "pcmu2";
    
    const char *hubEx = "ndn/ucla.edu/apps";
    char *userEx = "testuser";
    char *streamEx = "audio0";
    
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/key", hubEx, userEx, streamEx);
        
        string prefixS;
        NdnAudioSender::getStreamKeyPrefix(p, prefixS);
        EXPECT_STREQ(prefix, prefixS.c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/pcmu2/frames", hubEx, userEx, streamEx);
        
        string prefixS;
        NdnAudioSender::getStreamFramePrefix(p, prefixS);
        EXPECT_STREQ(prefix, prefixS.c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/pcmu2/control", hubEx, userEx, streamEx);
        
        string prefixS;
        NdnAudioSender::getStreamControlPrefix(p, prefixS);
        EXPECT_STREQ(prefix, prefixS.c_str());
    }
}

TEST(AudioData, TestCreateDelete)
{
    {
        unsigned int dataSz = 170;
        unsigned char *dummyData = (unsigned char*)malloc(dataSz);
        
        for (int i = 0; i < dataSz; i++) dummyData[i] = (char)i;
        
        bool isRTCP = false;
    
        NdnAudioData::AudioPacket p = {isRTCP, dataSz, dummyData};
        ASSERT_NO_THROW(
                        NdnAudioData data(p);
                        );
        
        NdnAudioData data(p);
        
        EXPECT_LT(dataSz, data.getLength());
        
        unsigned char *packetData = data.getData();
        EXPECT_TRUE(NULL != packetData);
    }
}
TEST(AudioData, TestPackUnpack)
{
    {
        unsigned int dataSz = 170;
        unsigned char *dummyData = (unsigned char*)malloc(dataSz);
        
        for (int i = 0; i < dataSz; i++) dummyData[i] = (char)i;
        
        bool isRTCP_exp = false;
        
        NdnAudioData::AudioPacket p = {isRTCP_exp, dataSz, dummyData};
        NdnAudioData data(p);
        
        NdnAudioData::AudioPacket resP;
        ASSERT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data.getLength(),
                                                       data.getData(), resP));
        
        EXPECT_EQ(isRTCP_exp, resP.isRTCP_);
        EXPECT_EQ(dataSz, resP.length_);
        EXPECT_FALSE(NULL == resP.data_);
        
        for (int i = 0; i < resP.length_; i++)
            EXPECT_EQ(dummyData[i], resP.data_[i]);

    }
    {
        unsigned int dataSz = 170;
        unsigned char *dummyData = (unsigned char*)malloc(dataSz);
        
        for (int i = 0; i < dataSz; i++) dummyData[i] = (char)i;
        
        bool isRTCP_exp = true;
        
        NdnAudioData::AudioPacket p = {isRTCP_exp, dataSz, dummyData};
        NdnAudioData data(p);

        NdnAudioData::AudioPacket resP;
        ASSERT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data.getLength(),
                                                       data.getData(), resP));
        
        EXPECT_EQ(isRTCP_exp, resP.isRTCP_);
        EXPECT_EQ(dataSz, resP.length_);
        EXPECT_FALSE(NULL == resP.data_);
        
        for (int i = 0; i < resP.length_; i++)
            EXPECT_EQ(dummyData[i], resP.data_[i]);
    }
}
TEST(AudioData, TestUnpackError)
{
    {
        unsigned int dataSz = 170;
        unsigned char *dummyData = (unsigned char*)malloc(dataSz);
        
        for (int i = 0; i < dataSz; i++) dummyData[i] = (char)i;
        
        
        unsigned int dataLength = 0;
        bool isRTCP;
        unsigned char *packetData = NULL;
        
        NdnAudioData::AudioPacket resP;
        ASSERT_EQ(RESULT_ERR, NdnAudioData::unpackAudio(dataSz,
                                                       dummyData, resP));
    }
}

class AudioSenderTester : public webrtc::Transport, public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        setupWebRTCLogging();
        
        shared_ptr<ndn::Transport::ConnectionInfo> connInfo(new ndn::TcpTransport::ConnectionInfo("localhost", 6363));
        ndnTransport_.reset(new TcpTransport());
        ndnFace_.reset(new Face(ndnTransport_, connInfo));
        params_ = DefaultParams;
        params_.freshness = 10;
        
        std::string streamAccessPrefix;
        MediaSender::getStreamKeyPrefix(params_, streamAccessPrefix);
        
        ASSERT_NO_THROW(
                        ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                                 bind(&AudioSenderTester::onInterest, this, _1, _2, _3),
                                                 bind(&AudioSenderTester::onRegisterFailed, this, _1));
                        );
        
        NdnRtcObjectTestHelper::SetUp();
        config_.Set<webrtc::AudioCodingModuleFactory>(new webrtc::NewAudioCodingModuleFactory());
        voiceEngine_ = webrtc::VoiceEngine::Create(config_);
        
        ASSERT_TRUE(voiceEngine_ != NULL);
        
        voe_base_ = webrtc::VoEBase::GetInterface(voiceEngine_);
        voe_network_ = webrtc::VoENetwork::GetInterface(voiceEngine_);
        voe_base_->Init();
        
        sender_ = new NdnAudioSender(params_);
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (voiceEngine_ != NULL)
        {
            voe_base_->Release();
            voe_network_->Release();
            webrtc::VoiceEngine::Delete(voiceEngine_);
            
            if (sender_)
                delete sender_;
        }
    }
    
    int SendPacket(int channel, const void *data, int len)
    {
        INFO("publish rtp packet %d", rtcpSent_);
        rtpSent_++;
        sender_->publishRTPAudioPacket(len, (unsigned char*)data);
        
        return len;
    }
    int SendRTCPPacket(int channel, const void *data, int len)
    {
        INFO("publish rtcp packet %d", rtcpSent_);
        rtcpSent_++;
        sender_->publishRTCPAudioPacket(len, (unsigned char*)data);

        return len;
    }
    
    void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport)
    {
        INFO("got interest: %s", interest->getName().toUri().c_str());
    }
    
    void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        FAIL();
    }
    
#if 0
    void onRTPData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
    {
        INFO("Got data packet with name %s, size: %d", data->getName().to_uri().c_str(), data->getContent().size());
        
        rtpDataFetched_++;
    }

    void onRTCPData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
    {
        INFO("Got data packet with name %s, size: %d", data->getName().to_uri().c_str(), data->getContent().size());
        
        rtcpDataFetched_++;
    }
#endif
    
    void onData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
    {
        unsigned char *rawAudioData = (unsigned char*)data->getContent().buf();
        
        NdnAudioData::AudioPacket packet;
        
        EXPECT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data->getContent().size(),
                                                       rawAudioData, packet));
        
        if (packet.isRTCP_)
            rtcpDataFetched_++;
        else
            rtpDataFetched_++;
    }
    
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        timeoutReceived_ = true;
        INFO("Time out for interest %s", interest->getName().toUri().c_str());
    }
    
    static bool fetchThreadFunc(void *obj){
        return ((AudioSenderTester*)obj)->fetchData();
    }
    bool fetchData()
    {
        ndnFace_->processEvents();
        WAIT(10);
        
        return alive_;
    }
    
    
protected:
    bool timeoutReceived_ = false, alive_ = true;
    unsigned int rtpDataFetched_ = 0, rtcpDataFetched_ = 0;
    unsigned int rtpSent_ = 0;
    unsigned int rtcpSent_ = 0;
    NdnAudioSender *sender_ = NULL;
    ParamsStruct params_;
    
    int channel_;
    webrtc::VoiceEngine *voiceEngine_;
    webrtc::VoEBase *voe_base_;
    webrtc::VoENetwork *voe_network_;
    webrtc::Config config_;
    
    shared_ptr<TcpTransport> ndnTransport_;
    shared_ptr<Face> ndnFace_;
//    shared_ptr<KeyChain> ndnKeyChain_;
};

TEST_F(AudioSenderTester, TestSend)
{
    int nPackets = 100;
    
    sender_->init(ndnTransport_);
    channel_ = voe_base_->CreateChannel();
    
    TRACE("channel id %d", channel_);
    
    ASSERT_LE(0, channel_);
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *this));
    
    EXPECT_EQ(0, voe_base_->StartReceive(channel_));
    EXPECT_EQ(0, voe_base_->StartSend(channel_));
    EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
    
    EXPECT_TRUE_WAIT(rtpSent_ >= nPackets, 5000);
    
    EXPECT_EQ(0, voe_base_->StopSend(channel_));
    EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
    EXPECT_EQ(0, voe_base_->StopReceive(channel_));
    
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    
    webrtc::ThreadWrapper *fetchThread = webrtc::ThreadWrapper::CreateThread(fetchThreadFunc, this);
    unsigned int tid = 1;
    ASSERT_NE(0,fetchThread->Start(tid));
    
    // now check what we have on the network
    string rtpPrefix, rtcpPrefix;
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamFramePrefix(params_,
                                                              rtpPrefix));
    EXPECT_EQ(RESULT_OK, NdnAudioSender::getStreamControlPrefix(params_,
                                                                rtcpPrefix));
    
    Name rtpPacketPrefix(rtpPrefix);
    Name rtcpPacketPrefix(rtcpPrefix);
    
    for (int i = 0; i < rtpSent_+rtcpSent_; i++)
    {
        Name prefix = rtpPacketPrefix;
        
        char frameNoStr[3];
        memset(&frameNoStr[0], 0, 3);
        sprintf(&frameNoStr[0], "%d", i);
        
        prefix.addComponent((const unsigned char*)&frameNoStr[0], strlen(frameNoStr));
        
        INFO("expressing %s", prefix.toUri().c_str());
        ndnFace_->expressInterest(prefix, bind(&AudioSenderTester::onData, this, _1, _2),
                                  bind(&AudioSenderTester::onTimeout, this, _1));
    }
#if 0
    for (int i = 0; i < rtpSent_; i++)
    {
        Name prefix = rtpPacketPrefix;
        
        char frameNoStr[3];
        memset(&frameNoStr[0], 0, 3);
        sprintf(&frameNoStr[0], "%d", i);
        
        prefix.addComponent((const unsigned char*)&frameNoStr[0], strlen(frameNoStr));
        
        INFO("expressing %s", prefix.toUri().c_str());
        ndnFace_->expressInterest(prefix, bind(&AudioSenderTester::onRTPData, this, _1, _2),
                                  bind(&AudioSenderTester::onTimeout, this, _1));
    }
    
    for (int j = 0; j < rtcpSent_; j++)
    {
        Name prefix = rtcpPacketPrefix;
        
        char frameNoStr[3];
        memset(&frameNoStr[0], 0, 3);
        sprintf(&frameNoStr[0], "%d", j);
        
        prefix.addComponent((const unsigned char*)&frameNoStr[0], strlen(frameNoStr));
        
        INFO("expressing %s", prefix.toUri().c_str());
        ndnFace_->expressInterest(prefix, bind(&AudioSenderTester::onRTCPData, this, _1, _2),
                                  bind(&AudioSenderTester::onTimeout, this, _1));
    }
#endif
    
    EXPECT_TRUE_WAIT(rtpDataFetched_ == rtpSent_ && rtcpDataFetched_ == rtcpSent_, 5000);
    
    alive_ = false;
    fetchThread->SetNotAlive();
    fetchThread->Stop();
    
    EXPECT_EQ(rtpSent_, rtpDataFetched_);
    EXPECT_EQ(rtcpSent_, rtcpDataFetched_);
}
