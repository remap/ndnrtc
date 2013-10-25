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
#include <iostream.h>

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR

#define NDN_DETAILED
#define NDN_TRACE
#define NDN_DEBUG

using namespace ndnrtc;
using namespace webrtc;

#define USE_RECEIVER_FACE

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(AudioReceiverParamsTest, CheckPrefixes)
{
    
}

class AudioReceiverTester : public NdnRtcObjectTestHelper,
public IAudioPacketConsumer,
public webrtc::Transport
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        shared_ptr<ndn::Transport::ConnectionInfo> connInfo(new ndn::TcpTransport::ConnectionInfo("localhost", 6363));
        ndnTransport_.reset(new TcpTransport());
        ndnFace_.reset(new Face(ndnTransport_, connInfo));
        params_ = DefaultParams;
        
        std::string streamAccessPrefix;
        MediaSender::getStreamKeyPrefix(params_, streamAccessPrefix);
        
        ASSERT_NO_THROW(
                        ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                                 bind(&AudioReceiverTester::onInterest, this, _1, _2, _3),
                                                 bind(&AudioReceiverTester::onRegisterFailed, this, _1));
                        );
        
        
#ifdef USE_RECEIVER_FACE
        shared_ptr<ndn::Transport::ConnectionInfo> connInfoTcp(new TcpTransport::ConnectionInfo("localhost", 6363));
        ndnReceiverFace_.reset(new Face(shared_ptr<ndn::Transport>(new ndn::TcpTransport()), connInfoTcp));
        
        streamAccessPrefix += "/receiver";
        ndnReceiverFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                         bind(&AudioReceiverTester::onInterest, this, _1, _2, _3),
                                         bind(&AudioReceiverTester::onRegisterFailed, this, _1));
#else
        ndnReceiverFace_ = ndnFace_;
#endif
        
        string userPrefix;
        ASSERT_EQ(RESULT_OK,MediaSender::getUserPrefix(params_, userPrefix));
        
        ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix);
        certName_ = NdnRtcNamespace::certificateNameForUser(userPrefix);
        
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
        NdnAudioData::AudioPacket p = {false, (unsigned int)len, (unsigned char*)data};
        NdnAudioData adata(p);
        NdnAudioSender::getStreamFramePrefix(params_, rtpPrefix);
        
        sendCS_->Enter();
        publishData(currentRTPFrame_++, params_.segmentSize,
                    adata.getLength(), adata.getData(), rtpPrefix, params_.freshness);
        sendCS_->Leave();
        
        nSent_++;
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
        publishData(currentRTCPFrame_++, params_.segmentSize,
                    adata.getLength(), adata.getData(), rtcpPrefix, params_.freshness);
#else
        publishData(currentRTPFrame_++, params_.segmentSize,
                    adata.getLength(), adata.getData(), rtcpPrefix, params_.freshness);
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
    
    void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport)
    {
        INFO("got interest: %s", interest->getName().toUri().c_str());
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
    ParamsStruct params_;
    shared_ptr<ndn::Transport> ndnTransport_;
    shared_ptr<Face> ndnFace_, ndnReceiverFace_;
    shared_ptr<KeyChain> ndnKeyChain_;
    shared_ptr<Name> certName_;
    
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
    
    void publishData(unsigned int frameNo,
                     unsigned int segmentSize,
                     unsigned int dataLen,
                     unsigned char *dataPacket,
                     string &framePrefix,
                     int freshness = DefaultParams.freshness,
                     bool mixedSendOrder = false)
    {
        unsigned int fullSegmentsNum = dataLen/segmentSize;
        unsigned int totalSegmentsNum = (dataLen - fullSegmentsNum*segmentSize)?
        fullSegmentsNum+1:fullSegmentsNum;
        
        unsigned int lastSegmentSize = dataLen - fullSegmentsNum*segmentSize;
        vector<int> segmentsSendOrder;
        
        Name prefix(framePrefix.c_str());
        shared_ptr<const vector<unsigned char>> frameNumberComponent =
        NdnRtcNamespace::getNumberComponent(frameNo);
        
        prefix.addComponent(*frameNumberComponent);
        
        // setup send order for segments
        for (int i = 0; i < totalSegmentsNum; i++)
            segmentsSendOrder.push_back(i);
        
        if (mixedSendOrder)
            random_shuffle(segmentsSendOrder.begin(), segmentsSendOrder.end());
        
        for (int i = 0; i < totalSegmentsNum; i++)
        {
            unsigned int segmentIdx = segmentsSendOrder[i];
            unsigned char *segmentData = dataPacket+segmentIdx*segmentSize;
            unsigned int dataSize = (segmentIdx == totalSegmentsNum -1)?lastSegmentSize:segmentSize;
            
            if (dataSize > 0)
            {
                Name segmentPrefix = prefix;
                segmentPrefix.appendSegment(segmentIdx);
                
                Data data(segmentPrefix);
                
                data.getMetaInfo().setFreshnessSeconds(freshness);
                data.getMetaInfo().setFinalBlockID(Name().appendSegment(totalSegmentsNum-1).get(0).getValue());
                data.getMetaInfo().setTimestampMilliseconds(millisecondTimestamp());
                data.setContent(segmentData, dataSize);
                
                ndnKeyChain_->sign(data, *certName_);
                
                ASSERT_TRUE(ndnTransport_->getIsConnected());
                
                Blob encodedData = data.wireEncode();
                ndnTransport_->send(*encodedData);
            } // if
            
        } // for
    }
};

TEST_F(AudioReceiverTester, TestFetch)
{
    params_.freshness = 1;
    params_.streamName = "audio0";
    params_.streamThread = "pcmu2";
    params_.bufferSize = 5;
    params_.slotSize = 1000;
    
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
    
    unsigned int publishPacketsNum = 50;
    EXPECT_TRUE_WAIT(nSent_ >= publishPacketsNum, 10000);
    
    EXPECT_TRUE_WAIT(nReceived_ == nSent_, 10000);
    EXPECT_EQ(nRTCPSent_, nRTCPReceived_);
    
    EXPECT_EQ(0, voe_base_->StopSend(channel_));
    EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
    EXPECT_EQ(0, voe_base_->StopReceive(channel_));
    EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));

    EXPECT_EQ(RESULT_OK, receiver.stopFetching());
}