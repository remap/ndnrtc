//
//  video-encoder-test.cc
//  libndnrtc
//
//  Created by Peter Gusev on 2/20/15.
//  Copyright (c) 2015 REMAP. All rights reserved.
//

#include <stdio.h>
#include <vector>
#include <algorithm>
#include <gtest/gtest.h>

#include "video-coder.h"
#include "video-decoder.h"
#include "frame-data.h"
#include "ndnrtc-testing.h"

using namespace ndnrtc::new_api;
using namespace ndnrtc::testing;

class EncoderTests : public ::testing::Test, public IEncoderDelegate, public ndnrtc::IRawFrameConsumer
{
public:
    EncoderTests():writer("bin/unit-tests/decoded_640x480.yuv") {
        VideoCoderParams coderParams;
        
        coderParams.codecFrameRate_ = 30;
        coderParams.gop_ = 30;
        coderParams.startBitrate_ = 1500;
        coderParams.maxBitrate_ = 1500;
        coderParams.encodeWidth_ = 640;
        coderParams.encodeHeight_ = 480;
        coderParams.dropFramesOn_ = true;
        
        EXPECT_EQ(RESULT_OK, decoder.init(coderParams));
        EXPECT_EQ(RESULT_OK, coder.init(coderParams));
    }
    
    ~EncoderTests(){
        std::cout << "total read frames: " << nReadFrames_ << std::endl
        << "total encoded frames: " << nEncodedFrames_ << std::endl
        << "total Key encoded frames: " << nKeyEncoded_ << std::endl
        << "total Delta encoded frames: " << nDeltaEncoded_ << std::endl
        << "total dropped frames while encoding: " << nDroppedFrames_ << std::endl
        << "total decoded frames: " << nDecodedFrames_ << std::endl
        << "simulated drops: " << nSimulatedDropped_ << std::endl
        << "dropped delta: " << nDroppedDelta_ << std::endl;
    }
    
    void
    onEncodingStarted(){}
    
    void
    onFrameDropped(){ nDroppedFrames_++; }
    
    void
    onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                            double captureTimestamp,
                            bool completeFrame = true)
    {
        ASSERT_EQ(completeFrame, true);
        nEncodedFrames_++;
        
        if (encodedImage._frameType == webrtc::kKeyFrame) nKeyEncoded_++;
        else nDeltaEncoded_++;
        
        unsigned char *buffer = (unsigned char*)malloc(encodedImage._length);
        memcpy(buffer, encodedImage._buffer, encodedImage._length);
        webrtc::EncodedImage copiedFrame(buffer, encodedImage._length, encodedImage._length);
        copiedFrame._encodedWidth = encodedImage._encodedWidth;
        copiedFrame._encodedHeight = encodedImage._encodedHeight;
        copiedFrame._timeStamp = encodedImage._timeStamp;
        copiedFrame.ntp_time_ms_ = encodedImage.ntp_time_ms_;
        copiedFrame.capture_time_ms_ = encodedImage.capture_time_ms_;
        copiedFrame._frameType = encodedImage._frameType;
        copiedFrame._completeFrame = encodedImage._completeFrame;
        
        encodedFrames_.push_back(copiedFrame);
    }
    
    void
    onDeliverFrame(WebRtcVideoFrame &frame,
                   double unixTimeStamp)
    {
        nDecodedFrames_++;
        frame_.CopyFrame(frame);
        writer.writeFrame(frame_, true);
    }
    
    int nReadFrames_= 0, nEncodedFrames_ = 0, nDecodedFrames_ = 0, nDroppedFrames_ = 0;
    int nDeltaEncoded_ = 0, nKeyEncoded_ = 0;

    bool droppingStarted_ = false;
    int dropStartNo = 101;
    int nDrop = 60;
    int nSimulatedDropped_ = 0, nDroppedDelta_ = 0;
    
    WebRtcVideoFrame frame_;
    FrameWriter writer;
    NdnVideoDecoder decoder;
    VideoCoder coder;
    
    std::vector<webrtc::EncodedImage> encodedFrames_;
};

TEST_F(EncoderTests, Test1)
{
    coder.setFrameConsumer(this);
    decoder.setFrameConsumer(this);
    
    FrameReader frameReader("bin/unit-tests/captured_640x480.i420");
    WebRtcVideoFrame frame;
    int i = 0;
    
    std::cout << "start reading file " << std::endl;
    
    while (frameReader.readFrame(frame) >= 0)
    {
        nReadFrames_++;
        coder.onDeliverFrame(frame, 0);
    }
    
    // mess around with encoded images
    
    // 1. swap two delta frames
//    std::iter_swap(encodedFrames_.begin()+61, encodedFrames_.begin()+62);
    
    // 2. swap delta and key
//    std::iter_swap(encodedFrames_.begin()+59, encodedFrames_.begin()+60);
    
    // 3. corrupt some frames inside GOP, BUT do not notify decoder about this
    if (0)
    {
        std::vector<webrtc::EncodedImage>::iterator it = encodedFrames_.begin()+61;
        while (it < encodedFrames_.begin()+89) {
            webrtc::EncodedImage image = *it;
            unsigned int len = image._length;
            unsigned char *buffer = image._buffer;
            unsigned corruptedLen =  (int)(0.5*(double)len);
            
            memset(buffer+len-corruptedLen, 0, corruptedLen);
            it++;
        }
    }

    // 4. corrupt some frames inside GOP, AND notify decoder about this
    if (1)
    {
        std::vector<webrtc::EncodedImage>::iterator it = encodedFrames_.begin()+60;
        while (it < encodedFrames_.begin()+91) {
            webrtc::EncodedImage &image = *it;
            unsigned int len = image._length;
            unsigned char *buffer = image._buffer;
            unsigned corruptedLen =  (int)(0.3*(double)len);
            
            image._completeFrame = false;
            memset(buffer+len-corruptedLen, 0, corruptedLen);
            it++;
        }
    }
    
    bool skip = false;
    for (std::vector<webrtc::EncodedImage>::iterator it = encodedFrames_.begin();
         it != encodedFrames_.end(); ++it)
    {
        webrtc::EncodedImage& image = *it;
        
        if (skip && image._frameType == webrtc::kKeyFrame)
            skip = false;
        
        if (!image._completeFrame)
            skip = true;
        
        if (!skip)
            decoder.onEncodedFrameDelivered(image, image.capture_time_ms_, image._completeFrame);
    }
}


int
main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
