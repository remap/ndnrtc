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
TEST(VideoCoderParamsTest, CheckDefaults)
{
    ParamsStruct p = DefaultParams;
    
    EXPECT_EQ(30, p.codecFrameRate);
    EXPECT_EQ(1000, p.maxBitrate);
    EXPECT_EQ(100, p.startBitrate);
    EXPECT_EQ(640, p.encodeWidth);
    EXPECT_EQ(480, p.encodeHeight);
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
        
        obtainedFramesCount_= 0;
        bitrateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        NdnRtcUtils::releaseDataRateMeter(bitrateMeter_);
    }
    
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        obtainedFrame_ = true;
        obtainedFramesCount_++;
        
        receivedEncodedFrame_ = &encodedImage;
        receivedBytes_ += encodedImage._length;
        NdnRtcUtils::dataRateMeterMoreData(bitrateMeter_, encodedImage._length);
        
        cout << frameNo_ << "\t" << getBitratePerSec() << "\t" << encodedImage._length << endl;
    }
    
protected:
    webrtc::EncodedImage *receivedEncodedFrame_;
    int obtainedFramesCount_ = 0;
    bool obtainedFrame_ = false;
    int receivedBytes_ = 0, frameNo_;
    ParamsStruct coderParams_ = DefaultParams;
    webrtc::I420VideoFrame *sampleFrame_;
    int bitrateMeter_;
    
    void loadFrame()
    {
        int width = 352, height = 288;
        // change frame size according to the data in resource file file
        coderParams_.encodeWidth = width;
        coderParams_.encodeHeight = height;
        
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
    
    double getBitratePerSec(){
        return ((double)NdnRtcUtils::currentDataRateMeterValue(bitrateMeter_))*8./1024.;
    }
    
};
#if 0
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
        LOG_TRACE("can't get deafult params");
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

    NdnVideoCoder *vc = new NdnVideoCoder(DefaultParams);
    
    if (!encoder_)
        ASSERT_TRUE(false);
    
    int maxPayload = 3900;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        ASSERT_TRUE(false);
    
    encoder_->RegisterEncodeCompleteCallback(vc);
    EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->Encode(*sampleFrame_, NULL, NULL));
    WAIT(1000.);

}

TEST_F(NdnVideoCoderTest, TestEncodeSequence)
{
    FrameReader fr("resources/bipbop30.yuv");
    
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    vc->setObserver(this);
    vc->setFrameConsumer(this);
    vc->init();
    
    flushFlags();
    webrtc::I420VideoFrame frame;
    int nFrames = 0, delayedFrames = 0;
    uint64_t lostTime = 0;
    
    while (fr.readFrame(frame))
    {
        nFrames++;
        obtainedFrame_ = false;
        vc->onDeliverFrame(frame);
        usleep(30000);

        EXPECT_TRUE(obtainedFrame_);
        
        if (!obtainedFrame_)
        {
            lostTime += 30;
            delayedFrames++;
        }
        
        LOG_TRACE("frame no: %d, obtained: %d", nFrames-1, obtainedFramesCount_);
    }
    
    EXPECT_EQ(nFrames, obtainedFramesCount_);
    LOG_TRACE("lost time - %d ms, delay: %d", lostTime, delayedFrames);
    
    delete vc;
}
#endif
TEST_F(NdnVideoCoderTest, TestEncodeSequenceDropping)
{
    FrameReader fr("resources/bipbop10.yuv");
    coderParams_.startBitrate = 150;
    coderParams_.maxBitrate = 200;
    coderParams_.dropFramesOn = true;
    
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    vc->setObserver(this);
    vc->setFrameConsumer(this);
    vc->init();
    
    flushFlags();
    webrtc::I420VideoFrame frame, nextFrame;
    int nFrames = 0, droppedFrames = 0;
    int64_t lostTime = 0;
    int64_t totalTime = 0;
    
    frameNo_ = 0;
    
    EXPECT_EQ(0, fr.readFrame(frame));
    
    while (fr.readFrame(nextFrame) >= 0)
    {
        nFrames++;
        obtainedFrame_ = false;
        vc->onDeliverFrame(frame);
        
        int64_t sleepIntervalMs = nextFrame.render_time_ms() - frame.render_time_ms();
        totalTime += sleepIntervalMs;
        usleep(sleepIntervalMs*1000);
        
        if (!coderParams_.dropFramesOn)
            EXPECT_TRUE(obtainedFrame_);
        
        if (!obtainedFrame_)
        {
            lostTime += sleepIntervalMs;
            droppedFrames++;
        }
        
        frameNo_++;
        frame.SwapFrame(&nextFrame);
        
        LOG_TRACE("frame no: %d, obtained: %d", nFrames-1, obtainedFramesCount_);
    }
    
    EXPECT_EQ(nFrames, obtainedFramesCount_);
    LOG_TRACE("total time %d ms, lost time - %d ms (%.2f%%), dropped frames: %d",
            totalTime, lostTime, (double)lostTime/(double)totalTime, droppedFrames);
    
    delete vc;
}
