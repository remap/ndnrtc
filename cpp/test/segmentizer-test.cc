//
//  segmentizer-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 2/25/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "segmentizer.h"
#include "ndnrtc-utils.h"
#include "frame-buffer.h"
#include <string.h>
#include "ndnrtc-namespace.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(SegmentizerTest, TestSegmentData)
{
    {
        unsigned int dataSize = 256;
        unsigned char *data = (unsigned char*)malloc(dataSize);

        for (int i = 0; i < dataSize; i++)
            data[i] = i;
        
        int64_t interestArrivalMs = NdnRtcUtils::millisecondTimestamp();
        int32_t generationDelayMs = std::rand();
        uint32_t nonce = std::rand();
        
        {
            SegmentData segData(data, dataSize);
            EXPECT_LT(dataSize, segData.getLength());
            
            SegmentData restoredData;
            
            EXPECT_EQ(RESULT_OK, SegmentData::segmentDataFromRaw(segData.getLength(), segData.getData(), restoredData));
            EXPECT_EQ(0, restoredData.getMetadata().interestArrivalMs_);
            EXPECT_EQ(0, restoredData.getMetadata().generationDelayMs_);
            EXPECT_EQ(0, restoredData.getMetadata().interestNonce_);
        }
        
        {
            SegmentData::SegmentMetaInfo meta;
            meta.interestNonce_ = nonce;
            meta.interestArrivalMs_ = interestArrivalMs;
            meta.generationDelayMs_ = generationDelayMs;
            
            SegmentData segData(data, dataSize, meta);
            EXPECT_LT(dataSize, segData.getLength());
            
            SegmentData restoredData;
            
            EXPECT_EQ(RESULT_OK, SegmentData::segmentDataFromRaw(segData.getLength(), segData.getData(), restoredData));
            EXPECT_EQ(interestArrivalMs, restoredData.getMetadata().interestArrivalMs_);
            EXPECT_EQ(generationDelayMs, restoredData.getMetadata().generationDelayMs_);
            EXPECT_EQ(nonce, restoredData.getMetadata().interestNonce_);
            
        }
        
        free(data);
    }
}

TEST(SegmentizerTest, TestSegmentation)
{
    {
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.timestamp_ = 1234;
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData frameData(*frame, meta);
        
        int segSize = 80;
        Segmentizer::SegmentList segments;
        Segmentizer::segmentize(frameData, segments, segSize);
        
        EXPECT_NE(RESULT_OK, segments.size());
        
        int expectedSegNum = ceil((double)frameData.getLength()/(double)segSize);
        EXPECT_LE(expectedSegNum, segments.size());
        
        PacketData *packet;
        EXPECT_EQ(RESULT_OK, Segmentizer::desegmentize(segments, &packet));
        
        EXPECT_TRUE(packet->isValid());
        EXPECT_EQ(frameData.getLength(), packet->getLength());
        EXPECT_EQ(PacketData::TypeVideo, packet->getType());
        EXPECT_EQ(meta.packetRate_, packet->getMetadata().packetRate_);
        EXPECT_EQ(meta.timestamp_, packet->getMetadata().timestamp_);
        
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnFrameData*>(packet)->getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(frame, &restoredFrame);
        
        delete frame;
    }
    {
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.timestamp_ = 1234;
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData frameData(*frame, meta);
        
        int segSize = 80;
        Segmentizer::SegmentList segments;
        Segmentizer::segmentize(frameData, segments, segSize);
        
        EXPECT_NE(RESULT_OK, segments.size());
        
        int expectedSegNum = ceil((double)frameData.getLength()/(double)segSize);
        EXPECT_LE(expectedSegNum, segments.size());
        
        unsigned char* packetData = (unsigned char*)malloc(frameData.getLength());
        int i = 0;
        int dataLength = 0;
        
        for (Segmentizer::SegmentList::iterator it = segments.begin();
             it != segments.end(); ++it)
        {
            memcpy(packetData+dataLength, (*it).getDataPtr(), (*it).getPayloadSize());
            dataLength += (*it).getPayloadSize();
        }
        
        PacketData *packet;
        EXPECT_TRUE(RESULT_GOOD(PacketData::packetFromRaw(dataLength,
                                                          packetData,
                                                          &packet)));
        
        EXPECT_TRUE(packet->isValid());
        EXPECT_EQ(frameData.getLength(), packet->getLength());
        EXPECT_EQ(PacketData::TypeVideo, packet->getType());
        EXPECT_EQ(meta.packetRate_, packet->getMetadata().packetRate_);
        EXPECT_EQ(meta.timestamp_, packet->getMetadata().timestamp_);
        
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnFrameData*>(packet)->getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(frame, &restoredFrame);
        
        delete frame;
    }
    {
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.timestamp_ = 1234;
        
        int64_t ts = NdnRtcUtils::millisecondTimestamp();
        NdnAudioData::AudioPacket p = {false, ts,
            100, (unsigned char*)malloc(100)};
        NdnAudioData data(p, meta);
        
        int segSize = 30;
        Segmentizer::SegmentList segments;
        Segmentizer::segmentize(data, segments, segSize);
        
        EXPECT_NE(RESULT_OK, segments.size());
        
        int expectedSegNum = ceil((double)data.getLength()/(double)segSize);
        EXPECT_LE(expectedSegNum, segments.size());
        
        PacketData *packet;
        EXPECT_EQ(RESULT_OK, Segmentizer::desegmentize(segments, &packet));
        
        EXPECT_TRUE(packet->isValid());
        EXPECT_EQ(data.getLength(), packet->getLength());
        EXPECT_EQ(PacketData::TypeAudio, packet->getType());
        EXPECT_EQ(meta.packetRate_, packet->getMetadata().packetRate_);
        EXPECT_EQ(meta.timestamp_, packet->getMetadata().timestamp_);
        
        NdnAudioData::AudioPacket restoredPacket;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnAudioData*>(packet)->getAudioPacket(restoredPacket));

        EXPECT_EQ(p.isRTCP_, restoredPacket.isRTCP_);
        EXPECT_EQ(meta.timestamp_, restoredPacket.timestamp_);
        EXPECT_EQ(p.length_, restoredPacket.length_);

        free(p.data_);
    }
    { // 1-segment data
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.timestamp_ = 1234;
        
        int64_t ts = NdnRtcUtils::millisecondTimestamp();
        NdnAudioData::AudioPacket p = {false, ts,
            100, (unsigned char*)malloc(100)};
        NdnAudioData data(p, meta);
        
        int segSize = 300;
        Segmentizer::SegmentList segments;
        Segmentizer::segmentize(data, segments, segSize);
        
        EXPECT_NE(RESULT_OK, segments.size());
        
        int expectedSegNum = ceil((double)data.getLength()/(double)segSize);
        EXPECT_LE(expectedSegNum, segments.size());
        
        PacketData *packet;
        EXPECT_EQ(RESULT_OK, Segmentizer::desegmentize(segments, &packet));
        
        EXPECT_TRUE(packet->isValid());
        EXPECT_EQ(data.getLength(), packet->getLength());
        EXPECT_EQ(PacketData::TypeAudio, packet->getType());
        EXPECT_EQ(meta.packetRate_, packet->getMetadata().packetRate_);
        EXPECT_EQ(meta.timestamp_, packet->getMetadata().timestamp_);
        
        NdnAudioData::AudioPacket restoredPacket;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnAudioData*>(packet)->getAudioPacket(restoredPacket));
        
        EXPECT_EQ(p.isRTCP_, restoredPacket.isRTCP_);
        EXPECT_EQ(meta.timestamp_, restoredPacket.timestamp_);
        EXPECT_EQ(p.length_, restoredPacket.length_);
        
        free(p.data_);
    }
}

