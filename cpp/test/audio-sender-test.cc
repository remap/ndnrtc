//
//  audio-sender-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

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
    char *userEx = (char*)"testuser";
    char *streamEx = (char*)"audio0";
    
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
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/pcmu2/frames/delta", hubEx, userEx, streamEx);
        
        string prefixS;
        NdnAudioSender::getStreamFramePrefix(p, prefixS);
        EXPECT_STREQ(prefix, prefixS.c_str());
    }
    {
        char prefix[256];
        memset(prefix, 0, 256);
        sprintf(prefix, "/%s/ndnrtc/user/%s/streams/%s/pcmu2/frames/key", hubEx, userEx, streamEx);
        
        string prefixS;
        NdnAudioSender::getStreamFramePrefix(p, prefixS, true);
        EXPECT_STREQ(prefix, prefixS.c_str());
    }
    if (0) // remove this check for now - RTP prefix equals RTCP
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
        int64_t timestamp_exp = NdnRtcUtils::millisecondTimestamp();
    
        NdnAudioData::AudioPacket p = {isRTCP, timestamp_exp, dataSz, dummyData};
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
        int64_t timestamp_exp = NdnRtcUtils::millisecondTimestamp();
        
        NdnAudioData::AudioPacket p = {isRTCP_exp, timestamp_exp,
            dataSz, dummyData};
        NdnAudioData data(p);
        
        NdnAudioData::AudioPacket resP;
        ASSERT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data.getLength(),
                                                       data.getData(), resP));
        
        EXPECT_EQ(isRTCP_exp, resP.isRTCP_);
        EXPECT_EQ(timestamp_exp, resP.timestamp_);
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
        int64_t timestamp_exp = NdnRtcUtils::millisecondTimestamp();
        
        NdnAudioData::AudioPacket p = {isRTCP_exp, timestamp_exp,
            dataSz, dummyData};
        NdnAudioData data(p);

        NdnAudioData::AudioPacket resP;
        ASSERT_EQ(RESULT_OK, NdnAudioData::unpackAudio(data.getLength(),
                                                       data.getData(), resP));
        
        EXPECT_EQ(isRTCP_exp, resP.isRTCP_);
        EXPECT_EQ(timestamp_exp, resP.timestamp_);
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

class AudioSenderTester : public webrtc::Transport,
public NdnRtcObjectTestHelper, public UnitTestHelperNdnNetwork
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        setupWebRTCLogging();
        
        params_ = DefaultParamsAudio;
        params_.streamName = "audio-sender-test";
        
        std::string streamAccessPrefix, userPrefix = "whatever";
        MediaSender::getStreamKeyPrefix(params_, streamAccessPrefix);
        
        UnitTestHelperNdnNetwork::NdnSetUp(streamAccessPrefix, userPrefix);
        
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
        UnitTestHelperNdnNetwork::NdnTearDown();
        
        if (voiceEngine_ != NULL)
        {
            voe_base_->Release();
            voe_network_->Release();
            webrtc::VoiceEngine::Delete(voiceEngine_);
            voiceEngine_ = NULL;
            
            if (sender_)
                delete sender_;
        }
    }
    
    int SendPacket(int channel, const void *data, int len)
    {
//        cout << "publish rtp packet " << len << endl;
        rtpSent_++;
        sender_->publishRTPAudioPacket(len, (unsigned char*)data);
        
        return len;
    }
    int SendRTCPPacket(int channel, const void *data, int len)
    {
//        cout << "publish rtcp packet " << len << endl;
        rtcpSent_++;
        sender_->publishRTCPAudioPacket(len, (unsigned char*)data);

        return len;
    }
    
    void onData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
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
    
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        
        timeoutReceived_ = true;
        LOG_INFO("Time out for interest %s", interest->getName().toUri().c_str());
    }
    
protected:
    bool timeoutReceived_ = false;
    unsigned int rtpDataFetched_ = 0, rtcpDataFetched_ = 0;
    unsigned int rtpSent_ = 0;
    unsigned int rtcpSent_ = 0;
    NdnAudioSender *sender_ = NULL;
    
    int channel_;
    webrtc::VoiceEngine *voiceEngine_;
    webrtc::VoEBase *voe_base_;
    webrtc::VoENetwork *voe_network_;
    webrtc::Config config_;
};

TEST_F(AudioSenderTester, TestAudioData)
{
    int64_t ts = NdnRtcUtils::millisecondTimestamp();
    NdnAudioData::AudioPacket p = {false, ts,
        100, (unsigned char*)malloc(100)};
    NdnAudioData data(p);
    
    {
        EXPECT_TRUE(NdnAudioData::isAudioData(data.getLength(), data.getData()));
        
        unsigned char *random = (unsigned char *)malloc(100);
        EXPECT_FALSE(NdnAudioData::isAudioData(100, random));
        free(random);
    }
    
    {
        EXPECT_EQ(ts, NdnAudioData::getTimestamp(data.getLength(), data.getData()));
        
        unsigned char *random = (unsigned char *)malloc(100);
        EXPECT_EQ(-1, NdnAudioData::getTimestamp(100, random));
        free(random);
    }
    
    free(p.data_);
}

TEST_F(AudioSenderTester, TestSend)
{
    int nPackets = 100;
    
    params_.freshness =  (int)(((double)20*(double)nPackets*2)/1000.);
    
    sender_->init(ndnTransport_);
    channel_ = voe_base_->CreateChannel();
    
    ASSERT_LE(0, channel_);
    EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *this));
    
    EXPECT_EQ(0, voe_base_->StartReceive(channel_));
    EXPECT_EQ(0, voe_base_->StartSend(channel_));
    EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
    
    EXPECT_TRUE_WAIT(rtpSent_+rtcpSent_ >= nPackets, 20*nPackets);
    
    EXPECT_EQ(0, voe_base_->StopSend(channel_));
    EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
    EXPECT_EQ(0, voe_base_->StopReceive(channel_));
    
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    
    UnitTestHelperNdnNetwork::startProcessingNdn();
    
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
        
        prefix.addComponent((const unsigned char*)&frameNoStr[0],
                            strlen(frameNoStr));
        
        LOG_INFO("expressing %s", prefix.toUri().c_str());
        ndnFace_->expressInterest(prefix, bind(&AudioSenderTester::onData, this, _1, _2),
                                  bind(&AudioSenderTester::onTimeout, this, _1));
    }
    
    EXPECT_TRUE_WAIT(rtpDataFetched_ == rtpSent_ &&
                     rtcpDataFetched_ == rtcpSent_, 2*20*nPackets);
    
    UnitTestHelperNdnNetwork::stopProcessingNdn();
    
    EXPECT_EQ(rtpSent_, rtpDataFetched_);
    EXPECT_EQ(rtcpSent_, rtcpDataFetched_);
    EXPECT_LT(rtcpSent_, rtpSent_);
    WAIT(params_.freshness*1000);
}

