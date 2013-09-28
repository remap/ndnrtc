//
//  video-decoder-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "video-decoder.h"
#include "renderer.h"
#include "camera-capturer.h"
#include "video-coder.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);

//********************************************************************************
class NdnVideoCoderTest : public NdnRtcObjectTestHelper, public IRawFrameConsumer
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();

        if (!webrtc::VCMCodecDataBase::Codec(VCM_VP8_IDX, &codec_))
        {
            TRACE("can't get deafult params");
            strncpy(codec_.plName, "VP8", 31);
            codec_.maxFramerate = 30;
            codec_.startBitrate  = 300;
            codec_.maxBitrate = 4000;
            codec_.width = 640;
            codec_.height = 480;
            codec_.plType = VCM_VP8_PAYLOAD_TYPE;
            codec_.qpMax = 56;
            codec_.codecType = webrtc::kVideoCodecVP8;
            codec_.codecSpecific.VP8.denoisingOn = true;
            codec_.codecSpecific.VP8.complexity = webrtc::kComplexityNormal;
            codec_.codecSpecific.VP8.numberOfTemporalLayers = 1;
        }

        
        obtainedFramesCount_= 0;
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (sampleFrame_)
            delete sampleFrame_;
    }
    
    void onDeliverFrame(webrtc::I420VideoFrame &frame)
    {
        decodedFrame_.CopyFrame(frame);
        obtainedFramesCount_ ++;
        obtainedFrame_ = true;
    }
    
protected:
    webrtc::VideoCodec codec_;
    webrtc::I420VideoFrame decodedFrame_;
    int obtainedFramesCount_ = 0;
    bool obtainedFrame_ = false;
    
    webrtc::EncodedImage *sampleFrame_ = NULL;
    
    void loadFrame()
    {
        int width = 640, height = 480;
        // change frame size according to the data in resource file file
        
        FILE *f = fopen("resources/vp8_640x480.frame", "rb");
        ASSERT_TRUE(f);
        
        int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
        
        
        int32_t size, length;
        ASSERT_TRUE(fread(&size, sizeof(size), 1, f));
        ASSERT_TRUE(fread(&length, sizeof(length), 1, f));
        
        unsigned char* frameData = new unsigned char[size];
        
        ASSERT_TRUE(fread(frameData, size, 1, f));
        
//#warning - temp hack (size instead of length)
        sampleFrame_ = new webrtc::EncodedImage(frameData, length, size);
        sampleFrame_->_encodedWidth = width;
        sampleFrame_->_encodedHeight = height;
        sampleFrame_->_frameType = webrtc::kKeyFrame;
        sampleFrame_->_completeFrame = true;
        
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
    EXPECT_NO_THROW(
                    NdnVideoDecoder *decoder = new NdnVideoDecoder();
                    delete decoder;
                    );
}
TEST_F(NdnVideoCoderTest, TestInit)
{
    NdnVideoDecoder  *decoder = new NdnVideoDecoder();
    EXPECT_EQ(0, decoder->init(codec_));
    delete decoder;
}
//TEST_F(NdnVideoCoderTest, TestDecode)
//{
//    loadFrame();
//    
//    NdnVideoDecoder  *decoder = new NdnVideoDecoder();
//
//    decoder->setObserver(this);
//    decoder->setFrameConsumer(this);
//    
//    decoder->init(codec_);
//    EXPECT_EQ(false, obtainedError_);
//    
//    decoder->onEncodedFrameDelivered(*sampleFrame_);
//    EXPECT_EQ(false, obtainedError_);
//    
//    EXPECT_TRUE_WAIT(obtainedFrame_, 5000);
//    
//    delete decoder;
//}
TEST_F(NdnVideoCoderTest, CaptureEncodeDecodeAndRender)
{
    CameraCapturer *cc = new CameraCapturer(CameraCapturerParams::defaultParams());
    NdnVideoCoder *coder = new NdnVideoCoder(NdnVideoCoderParams::defaultParams());
    NdnVideoDecoder  *decoder = new NdnVideoDecoder();
    NdnRenderer *renderer = new NdnRenderer(1, NdnRendererParams::defaultParams());
    
    EXPECT_EQ(0, cc->init());
    EXPECT_EQ(0, coder->init());
    EXPECT_EQ(0, decoder->init(codec_));
    EXPECT_EQ(0, renderer->init());
    
    cc->setFrameConsumer(coder);
    coder->setFrameConsumer(decoder);
    decoder->setFrameConsumer(renderer);
    
    cc->setObserver(this);
    coder->setObserver(this);
    decoder->setObserver(this);
    renderer->setObserver(this);
    
    EXPECT_EQ(false, obtainedError_);
    
    EXPECT_EQ(0, renderer->startRendering());
    EXPECT_EQ(0, cc->startCapture());
    
    EXPECT_EQ(false, obtainedError_);
    
    EXPECT_TRUE_WAIT(obtainedError_, 50000);
//    WAIT(5000);
    
    if (obtainedError_)
        INFO("got error %s", obtainedEmsg_);
    
    cc->stopCapture();
    
    delete cc;
    delete coder;
    delete decoder;
    delete renderer;
}