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
#if 0
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
            
            unsigned int dataSize = 0;
            unsigned char *data;
            SegmentData::SegmentMetaInfo meta;
            
            EXPECT_EQ(RESULT_OK, SegmentData::unpackSegmentData(segData.getData(),
                                                                             segData.getLength(),
                                                                             &data,
                                                                             dataSize,
                                                                             meta));
            EXPECT_EQ(0, meta.interestArrivalMs_);
            EXPECT_EQ(0, meta.generationDelayMs_);
            EXPECT_EQ(0, meta.interestNonce_);
        }
        
        {
            SegmentData::SegmentMetaInfo meta;
            meta.interestNonce_ = nonce;
            meta.interestArrivalMs_ = interestArrivalMs;
            meta.generationDelayMs_ = generationDelayMs;
            
            SegmentData segData(data, dataSize, meta);
            EXPECT_LT(dataSize, segData.getLength());
            
            unsigned int dataSize = 0;
            unsigned char *data;
            SegmentData::SegmentMetaInfo resMeta;
            
            EXPECT_EQ(RESULT_OK, SegmentData::unpackSegmentData(segData.getData(),
                                                                             segData.getLength(),
                                                                             &data,
                                                                             dataSize,
                                                                             resMeta));
            EXPECT_EQ(interestArrivalMs, resMeta.interestArrivalMs_);
            EXPECT_EQ(generationDelayMs, resMeta.generationDelayMs_);
            EXPECT_EQ(nonce, resMeta.interestNonce_);
            
        }
        
        free(data);
    }
}
#endif
TEST(SegmentizerTest, TestSegmentation)
{
    {
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.sequencePacketNumber_ = 1234;
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData frameData(*frame, meta);
        
        int segSize = 100;
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
        EXPECT_EQ(meta.sequencePacketNumber_, packet->getMetadata().sequencePacketNumber_);
        
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnFrameData*>(packet)->getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(frame, &restoredFrame);
        
        delete frame;
    }
    {
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.sequencePacketNumber_ = 1234;
        
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
        EXPECT_EQ(meta.sequencePacketNumber_, packet->getMetadata().sequencePacketNumber_);
        
        NdnAudioData::AudioPacket restoredPacket;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnAudioData*>(packet)->getAudioPacket(restoredPacket));

        EXPECT_EQ(p.isRTCP_, restoredPacket.isRTCP_);
        EXPECT_EQ(p.timestamp_, restoredPacket.timestamp_);
        EXPECT_EQ(p.length_, restoredPacket.length_);

        free(p.data_);
    }
    { // 1-segment data
        PacketData::PacketMetadata meta;
        meta.packetRate_ = 39;
        meta.sequencePacketNumber_ = 1234;
        
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
        EXPECT_EQ(meta.sequencePacketNumber_, packet->getMetadata().sequencePacketNumber_);
        
        NdnAudioData::AudioPacket restoredPacket;
        EXPECT_EQ(RESULT_OK, dynamic_cast<NdnAudioData*>(packet)->getAudioPacket(restoredPacket));
        
        EXPECT_EQ(p.isRTCP_, restoredPacket.isRTCP_);
        EXPECT_EQ(p.timestamp_, restoredPacket.timestamp_);
        EXPECT_EQ(p.length_, restoredPacket.length_);
        
        free(p.data_);
    }
}
#if 0
class SegmentizerTester : public NdnRtcObjectTestHelper,
public UnitTestHelperNdnNetwork
{
public:
    SegmentizerTester() :
    segmentizerCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
    publishingTimer_(*webrtc::EventWrapper::Create()),
    fetchingTimer_(*webrtc::EventWrapper::Create())
    {
    }
    
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        std::stringstream ss;
        std::srand(NdnRtcUtils::millisecondTimestamp());
        ss <<  "segmentizer-test-" << std::rand();
        string producer = ss.str();
        
        params_ = DefaultParams;
        params_.producerId = (char*)malloc(producer.size()+1);
        memset((void*)params_.producerId, 0, producer.size()+1);
        memcpy((void*)params_.producerId, producer.c_str(), producer.size());
        
        shared_ptr<string> keyPrefix = NdnRtcNamespace::getStreamKeyPrefix(params_),
                            userPrefix = NdnRtcNamespace::getUserPrefix(params_);
        
        NdnSetUp(*keyPrefix, *userPrefix);
        
        publishingPrefix_ = *NdnRtcNamespace::getStreamFramePrefix(params_);
        
        estimatorId_ = NdnRtcUtils::setupMeanEstimator(0);
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        NdnTearDown();
    }
    
    virtual void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& segmentData)
    {
        UnitTestHelperNdnNetwork::onData(interest, segmentData);
        
//        cout << NdnRtcUtils::millisecondTimestamp() << " new data " << segmentData->getName() << endl;
        
        unsigned int dataSize;
        unsigned char *data;
        SegmentData::SegmentMetaInfo metaInfo;
        
        SegmentData::unpackSegmentData(segmentData->getContent().buf(),
                                                    segmentData->getContent().size(),
                                                    &data, dataSize, metaInfo);
        
        int32_t sentNonce = NdnRtcUtils::blobToNonce(interest->getNonce());
        int32_t receivedNonce = metaInfo.interestNonce_;
        
        if (metaInfo.interestArrivalMs_ != 0 &&
            metaInfo.interestNonce_ != 0 &&
            receivedNonce == sentNonce)
        {
//            cout << "received meta from publisher. arrival "
//            << metaInfo.interestArrivalMs_
//            << " generation delay " << metaInfo.generationDelayMs_
//            << " name " << segmentData->getName() << endl;
            
            receivedMetaInfoFromPublisher_ = true;
            
            // if generation delay is growing, adjust our express interval
            if (metaInfo.generationDelayMs_ > lastReceivedGenerationDelay_)
            {
//                fetchingIntervalMs_ += metaInfo.generationDelayMs_;
            }
            lastReceivedGenerationDelay_ = metaInfo.generationDelayMs_;
        }
        
        double currentDelay = (double)(NdnRtcUtils::millisecondTimestamp() - lastTimeDataReceived_);
        
//        cout << nReceivedData_ << " " << currentDelay << endl;
        
        if (nReceivedData_ >= 2)
        {
            NdnRtcUtils::meanEstimatorNewValue(estimatorId_, currentDelay);
            
            cout << "arrival current " << currentDelay
            << " mean " << NdnRtcUtils::currentMeanEstimation(estimatorId_)
            << " deviation " << NdnRtcUtils::currentDeviationEstimation(estimatorId_) << endl;
        }

        lastTimeDataReceived_ = NdnRtcUtils::millisecondTimestamp();
    }
    
    virtual void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        
    }
    
    static bool fetchRoutine(void *object)
    {
        return ((SegmentizerTester*)object)->processIncomingData();
    }
    bool processIncomingData()
    {
        ndnReceiverFace_->processEvents();
        usleep(10);
        return isFetching_;
    }
    
protected:
    bool isPublishing_;
    string publishingPrefix_;
    int64_t publisherIntervalMs_;
    int64_t fetchingIntervalMs_;
    int publishedPacketNo_;
    int lastReceivedGenerationDelay_;
    int64_t lastTimeDataReceived_ = 0;

    unsigned int estimatorId_;
    
    bool receivedMetaInfoFromPublisher_;
    
    Segmentizer *segmentizer_;
    
    webrtc::CriticalSectionWrapper &segmentizerCs_;
    webrtc::EventWrapper &publishingTimer_, &fetchingTimer_;
    
    static bool publishingThreadRoutine(void *obj)
    {
        return ((SegmentizerTester*)obj)->publishSegment();
    }
    bool publishSegment()
    {
        {
            webrtc::CriticalSectionScoped scopedCs(&segmentizerCs_);
            
            int dataSize = 256;
            unsigned char *data =  (unsigned char*)malloc(dataSize);
            
            generateData(&data, dataSize);
            
            Name finalPrefix(publishingPrefix_);
            finalPrefix.appendSegment(publishedPacketNo_++);
            
            LOG_TRACE("publishing %s", finalPrefix.toUri().c_str());
            segmentizer_->publishData(finalPrefix, data, dataSize, 1);
            
            free(data);
        }
        
        usleep(publisherIntervalMs_*1000);
//        publishingTimer_.StartTimer(false, publisherIntervalMs_);
//        publishingTimer_.Wait(WEBRTC_EVENT_INFINITE);
        
        return isPublishing_;
    }
    
    void generateData(unsigned char **data, int size)
    {
        memset(*data, 0, size);
    }
};
//#if 0
TEST_F(SegmentizerTester, TestInit)
{
    segmentizer_ = new Segmentizer(params_);
    
    segmentizer_->setObserver(this);
    segmentizer_->setLogger(NdnLogger::sharedInstance());
    
    EXPECT_TRUE(RESULT_GOOD(segmentizer_->init(ndnFace_, ndnTransport_)));
    
    WAIT(1000);
    
    EXPECT_FALSE(obtainedError_);
    
    delete segmentizer_;
}
//#endif
//#if 0
TEST_F(SegmentizerTester, Test1)
{
    segmentizer_ = new Segmentizer(params_);

    segmentizer_->setLogger(NdnLogger::sharedInstance());
    segmentizer_->init(ndnFace_, ndnTransport_);
    
    isPublishing_ = true;
    publisherIntervalMs_ = 30;
    publishedPacketNo_ = 0;
    lastReceivedGenerationDelay_ = -1;
    nReceivedTimeout_ = 0;
    
    webrtc::ThreadWrapper &thread = *webrtc::ThreadWrapper::CreateThread(SegmentizerTester::publishingThreadRoutine, this);
    webrtc::ThreadWrapper &fetchingThread = *webrtc::ThreadWrapper::CreateThread(SegmentizerTester::fetchRoutine, this);

    
    unsigned int tid;
    thread.Start(tid);
    fetchingThread.Start(tid);
    WAIT(100);
    
    bool stopFetching = false;
    fetchingIntervalMs_ = publisherIntervalMs_/2;
    int segmentNo = 0;
    receivedMetaInfoFromPublisher_ = false;
    
    startProcessingNdn();
    
    while (!stopFetching)
    {
        Name fetchingPrefix(publishingPrefix_);
        fetchingPrefix.appendSegment(segmentNo);
        
        Interest i(fetchingPrefix);
        i.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
            
        ndnReceiverFace_->expressInterest(i,
                                          bind(&SegmentizerTester::onData, this, _1, _2),
                                          bind(&SegmentizerTester::onTimeout, this, _1));
        
        LOG_TRACE("expressing %s", fetchingPrefix.toUri().c_str());
        
        usleep(fetchingIntervalMs_*1000);
//        fetchingTimer_.StartTimer(false, fetchingIntervalMs_);
//        fetchingTimer_.Wait(fetchingIntervalMs_);
        
        stopFetching = (lastReceivedGenerationDelay_ == 0);
        
        segmentNo++;
    }
 
    stopProcessingNdn();
    
    thread.SetNotAlive();
    fetchingThread.SetNotAlive();
    isPublishing_ = false;
    thread.Stop();
    fetchingThread.Stop();
}
#endif
