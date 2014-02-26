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
            Segmentizer::SegmentData segData(data, dataSize);
            EXPECT_LT(dataSize, segData.getLength());
            
            unsigned int dataSize = 0;
            unsigned char *data;
            Segmentizer::SegmentData::SegmentMetaInfo meta;
            
            EXPECT_EQ(RESULT_OK, Segmentizer::SegmentData::unpackSegmentData(segData.getData(),
                                                                             segData.getLength(),
                                                                             &data,
                                                                             dataSize,
                                                                             meta));
            EXPECT_EQ(0, meta.interestArrivalMs_);
            EXPECT_EQ(0, meta.generationDelayMs_);
            EXPECT_EQ(0, meta.interestNonce_);
        }
        
        {
            Segmentizer::SegmentData::SegmentMetaInfo meta;
            meta.interestNonce_ = nonce;
            meta.interestArrivalMs_ = interestArrivalMs;
            meta.generationDelayMs_ = generationDelayMs;
            
            Segmentizer::SegmentData segData(data, dataSize, meta);
            EXPECT_LT(dataSize, segData.getLength());
            
            unsigned int dataSize = 0;
            unsigned char *data;
            Segmentizer::SegmentData::SegmentMetaInfo resMeta;
            
            EXPECT_EQ(RESULT_OK, Segmentizer::SegmentData::unpackSegmentData(segData.getData(),
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

class SegmentizerTester : public NdnRtcObjectTestHelper,
public UnitTestHelperNdnNetwork
{
public:
    SegmentizerTester() :
    segmentizerCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
    publishingTimer_(*webrtc::EventWrapper::Create())
    {
    }
    
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        std::stringstream ss;
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
    }
    
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        NdnTearDown();
    }
    
protected:
    bool isPublishing_;
    string publishingPrefix_;
    int64_t publisherIntervalMs_;
    int64_t fetchingIntervalMs_;
    int publishedPacketNo_;
    
    Segmentizer *segmentizer_;
    
    webrtc::CriticalSectionWrapper &segmentizerCs_;
    webrtc::EventWrapper &publishingTimer_;
    
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
            
            cout << "publishing " << finalPrefix << endl;
            segmentizer_->publishData(finalPrefix, data, dataSize);
            
            free(data);
        }
        
        publishingTimer_.StartTimer(false, publisherIntervalMs_);
        publishingTimer_.Wait(WEBRTC_EVENT_INFINITE);
        
        return isPublishing_;
    }
    
    void generateData(unsigned char **data, int size)
    {
        memset(*data, 0, size);
    }
    
    virtual void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data)
    {
        UnitTestHelperNdnNetwork::onData(interest, data);
    }
    
    virtual void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        
    }
};

TEST_F(SegmentizerTester, Test1)
{
    segmentizer_ = new Segmentizer(params_);
    
    segmentizer_->init(ndnFace_, ndnTransport_);
    isPublishing_ = true;
    publisherIntervalMs_ = 30;
    publishedPacketNo_ = 0;
    
    webrtc::ThreadWrapper &thread = *webrtc::ThreadWrapper::CreateThread(SegmentizerTester::publishingThreadRoutine, this);
    
    unsigned int tid;
    thread.Start(tid);
    
//    WAIT(5000);
    
    thread.SetNotAlive();
    isPublishing_ = false;
    thread.Stop();
    
}