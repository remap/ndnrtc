//
//  video-decoder.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

#include <boost/thread.hpp>

#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>

#include "video-decoder.hpp"
#include "video-coder.hpp"
#include "clock.hpp"

using namespace std;
using namespace ndnrtc;

//********************************************************************************
#pragma mark - construction/destruction
VideoDecoder::VideoDecoder(const VideoCoderParams& settings, OnDecodedImage onDecodedImage):
settings_(settings),
onDecodedImage_(onDecodedImage)
{
    memset(&codec_, 0, sizeof(codec_));
    
    codec_ = VideoCoder::codecFromSettings(settings_);
    
    stringstream ss;
    ss << "decoder-" << codec_.startBitrate;
    description_ = ss.str();
    
    resetDecoder();
}

//********************************************************************************
#pragma mark - public
void VideoDecoder::processFrame(const FrameInfo& frameInfo, const webrtc::EncodedImage& encodedImage)
{
    LogTraceC
        << " type " << (encodedImage._frameType == webrtc::kVideoFrameKey ? "KEY" : "DELTA")
        << " complete (encoder) " << encodedImage._completeFrame
        << std::endl;
    
    if (frameCount_ == 0)
        LogInfoC << "start decode from "
            << (encodedImage._frameType == webrtc::kVideoFrameKey ? "KEY" : "DELTA")
            << std::endl;
    
    frameCount_++;
    frameInfo_ = frameInfo;
    
    if (decoder_->Decode(encodedImage, true, NULL) != WEBRTC_VIDEO_CODEC_OK)
        LogErrorC << "error decoding " << endl;
    else
        LogTraceC << "decoded" << endl;
}

#pragma mark - private
void VideoDecoder::resetDecoder()
{
    frameCount_ = 0;
    if (decoder_.get())
        decoder_->Release();

#ifdef USE_VP9
    decoder_.reset(webrtc::VP9Decoder::Create());
#else
    decoder_.reset(webrtc::VP8Decoder::Create());
#endif
    
    if (!decoder_.get())
        throw std::runtime_error("can't create decoder");
    
    decoder_->RegisterDecodeCompleteCallback(this);
    
    if (decoder_->InitDecode(&codec_, boost::thread::hardware_concurrency()) != WEBRTC_VIDEO_CODEC_OK)
        throw std::runtime_error("can't initialize decoder");
}

#pragma mark - inteface implementation webrtc::DecodedImageCallback
int32_t VideoDecoder::Decoded(WebRtcVideoFrame &decodedImage)
{
    decodedImage.set_timestamp_us(clock::microsecondTimestamp());
    onDecodedImage_(frameInfo_, decodedImage);
    return 0;
}
