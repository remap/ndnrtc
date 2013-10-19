//
//  video-coder-test.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/29/13
//

#include "test-common.h"
#include "video-coder.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));


//********************************************************************************
/**
 * @name NdnVideoCoderParams class tests
 */
TEST(VideoCoderParamsTest, CreateDelete)
{
    NdnVideoCoderParams *p = NdnVideoCoderParams::defaultParams();
    delete p;
}
TEST(VideoCoderParamsTest, CheckDefaults)
{
    NdnVideoCoderParams *p = NdnVideoCoderParams::defaultParams();
    
    int width, height, framerate, maxbitrate, startbitrate;
    
    EXPECT_EQ(p->getFrameRate(&framerate),0);
    EXPECT_EQ(30, framerate);
    
//    EXPECT_EQ(p->getMinBitRate(&minbitrate),0);
//    EXPECT_EQ(framerate, 300);
    
    EXPECT_EQ(p->getMaxBitRate(&maxbitrate),0);
    EXPECT_EQ(4000, maxbitrate);
    
    EXPECT_EQ(p->getStartBitRate(&startbitrate),0);
    EXPECT_EQ(300, startbitrate);
    
    EXPECT_EQ(p->getWidth(&width),0);
    EXPECT_EQ(640, width);
    
    EXPECT_EQ(p->getHeight(&height),0);
    EXPECT_EQ(480, height);
    
    
    
    delete p;
}

//********************************************************************************
/**
 * @name NdnVideoCoder class tests
 */
class NdnVideoCoderTest : public NdnRtcObjectTestHelper, public IEncodedFrameConsumer
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        coderParams_ = NdnVideoCoderParams::defaultParams();
        obtainedFramesCount_= 0;
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        delete coderParams_;
    }
    
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        obtainedFrame_ = true;
        obtainedFramesCount_++;
        
        receivedEncodedFrame_ = &encodedImage;
    }
    
protected:
    webrtc::EncodedImage *receivedEncodedFrame_;
    int obtainedFramesCount_ = 0;
    bool obtainedFrame_ = false;
    NdnVideoCoderParams *coderParams_ = NULL;
    webrtc::I420VideoFrame *sampleFrame_;
    
    void loadFrame()
    {
        int width = 352, height = 288;
        // change frame size according to the data in resource file file
        coderParams_->setIntParam(NdnVideoCoderParams::ParamNameWidth, width);
        coderParams_->setIntParam(NdnVideoCoderParams::ParamNameHeight, height);
        
        FILE *f = fopen("resources/foreman_cif.yuv", "rb");
        ASSERT_TRUE(f);
        
        int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
        unsigned char* frameData = new unsigned char[frameSize];
        
        ASSERT_TRUE(fread(frameData, 1, frameSize, f));
        
        int size_y = width * height;
        int size_uv = ((width + 1) / 2)  * ((height + 1) / 2);
        sampleFrame_ = new webrtc::I420VideoFrame();
        
        ASSERT_EQ(sampleFrame_->CreateFrame(size_y, frameData,
                          size_uv,frameData + size_y,
                          size_uv, frameData + size_y + size_uv,
                          width, height,
                          width,
                          (width + 1) / 2, (width + 1) / 2), 0);
        
        fclose(f);
        delete [] frameData;
    }
    
    void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        
        obtainedFrame_ = false;
        obtainedFramesCount_ = 0;
    }
    
};

TEST_F(NdnVideoCoderTest, CreateDelete)
{
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    delete vc;
}
TEST_F(NdnVideoCoderTest, TestInit)
{
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    vc->setObserver(this);
    
    EXPECT_EQ(vc->init(),0);
    EXPECT_FALSE(obtainedError_);
    
    delete vc;
}
TEST_F(NdnVideoCoderTest, TestEncode)
{
    loadFrame();
    
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    vc->setObserver(this);
    vc->setFrameConsumer(this);
    vc->init();
    
    flushFlags();
    vc->onDeliverFrame(*sampleFrame_);
    EXPECT_TRUE_WAIT(obtainedFrame_, 1000.);
    
    EXPECT_EQ(sampleFrame_->width(),receivedEncodedFrame_->_encodedWidth);
    EXPECT_EQ(sampleFrame_->height(),receivedEncodedFrame_->_encodedHeight);
    
    delete sampleFrame_;
    delete vc;
}

#if 0
TEST(TestCodec, TestEncodeSampleFrame)
{
    int width = 352, height = 288;    
    FILE *f = fopen("resources/foreman_cif.yuv", "rb");
    
    int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
    unsigned char* frameData = new unsigned char[frameSize];
    
    fread(frameData, 1, frameSize, f);
    
    int size_y = width * height;
    int size_uv = ((width + 1) / 2)  * ((height + 1) / 2);
    webrtc::I420VideoFrame *sampleFrame_ = new webrtc::I420VideoFrame();
    
    sampleFrame_->CreateFrame(size_y, frameData,
                              size_uv,frameData + size_y,
                              size_uv, frameData + size_y + size_uv,
                              width, height,
                              width,
                              (width + 1) / 2, (width + 1) / 2);

    webrtc::VideoCodec codec_;
    webrtc::VideoEncoder *encoder_;
    
    if (!webrtc::VCMCodecDataBase::Codec(VCM_VP8_IDX, &codec_))
    {
        TRACE("can't get deafult params");
        strncpy(codec_.plName, "VP8", 31);
        codec_.maxFramerate = 30;
        codec_.startBitrate  = 300;
        codec_.maxBitrate = 4000;
        codec_.width = width;
        codec_.height = height;
        codec_.plType = VCM_VP8_PAYLOAD_TYPE;
        codec_.qpMax = 56;
        codec_.codecType = webrtc::kVideoCodecVP8;
        codec_.codecSpecific.VP8.denoisingOn = true;
        codec_.codecSpecific.VP8.complexity = webrtc::kComplexityNormal;
        codec_.codecSpecific.VP8.numberOfTemporalLayers = 1;
    }
    
    encoder_ = webrtc::VP8Encoder::Create();

    NdnVideoCoderParams *coderParams_ = NdnVideoCoderParams::defaultParams();
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    if (!encoder_)
        ASSERT_TRUE(false);
    
    int maxPayload = 3900;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        ASSERT_TRUE(false);
    
    encoder_->RegisterEncodeCompleteCallback(vc);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*sampleFrame_, NULL, NULL));
    WAIT(1000.);

}
#endif