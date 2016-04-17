//
//  video-coder.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

// #define USE_VP9

#include <boost/thread.hpp>
#include <webrtc/modules/video_coding/main/source/codec_database.h>
#include <webrtc/modules/video_coding/main/interface/video_coding_defines.h>
#include <modules/video_coding/main/source/internal_defines.h>
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>

#include "video-coder.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;

//********************************************************************************
char* plotCodec(webrtc::VideoCodec codec)
{
    static char msg[256];
    
    sprintf(msg, "\t\tMax Framerate:\t%d\n \
            \tStart Bitrate:\t%d\n \
            \tMin Bitrate:\t%d\n \
            \tMax Bitrate:\t%d\n \
            \tTarget Bitrate:\t%d\n \
            \tWidth:\t%d\n \
            \tHeight:\t%d",
            codec.maxFramerate,
            codec.startBitrate,
            codec.minBitrate,
            codec.maxBitrate,
            codec.targetBitrate,
            codec.width,
            codec.height);
    
    return msg;
}

//******************************************************************************
#pragma mark - static
webrtc::VideoCodec VideoCoder::codecFromSettings(const VideoCoderParams &settings)
{
    webrtc::VideoCodec codec;

    // setup default params first
#ifdef USE_VP9
    if (!webrtc::VCMCodecDataBase::Codec(VCM_VP9_IDX, &codec))
#else
    if (!webrtc::VCMCodecDataBase::Codec(VCM_VP8_IDX, &codec))
#endif
    {
        codec.maxFramerate = 30;
        codec.startBitrate  = 300;
        codec.maxBitrate = 4000;
        codec.width = 640;
        codec.height = 480;
        codec.qpMax = 56;
        
#ifdef USE_VP9
        strncpy(codec.plName, "VP9", 4);
        codec.plType = VCM_VP9_PAYLOAD_TYPE;
        codec.codecType = webrtc::kVideoCodecVP9;
        codec.codecSpecific.VP9.denoisingOn = true;
        codec.codecSpecific.VP9.complexity = webrtc::kComplexityNormal;
        codec.codecSpecific.VP9.numberOfTemporalLayers = 1;
        codec.codecSpecific.VP9.keyFrameInterval = settings.gop_;
#else
        strncpy(codec.plName, "VP8", 4);
        codec.plType = VCM_VP8_PAYLOAD_TYPE;
        codec.codecSpecific.VP8.resilience = kResilienceOff;
        codec.codecType = webrtc::kVideoCodecVP8;
        codec.codecSpecific.VP8.denoisingOn = true;
        codec.codecSpecific.VP8.complexity = webrtc::kComplexityNormal;
        codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
        codec.codecSpecific.VP8.keyFrameInterval = settings.gop_;
#endif
    }
    
    // dropping frames
#ifdef USE_VP9
    codec.codecSpecific.VP9.resilience = 1;
    codec.codecSpecific.VP9.frameDroppingOn = settings.dropFramesOn_;
    codec.codecSpecific.VP9.keyFrameInterval = settings.gop_;
#else
    codec.codecSpecific.VP8.resilience = kResilientStream;
    codec.codecSpecific.VP8.frameDroppingOn = settings.dropFramesOn_;
    codec.codecSpecific.VP8.keyFrameInterval = settings.gop_;
    codec.codecSpecific.VP8.feedbackModeOn = false;
#endif
    
    // customize parameteres if possible
    codec.maxFramerate = (int)settings.codecFrameRate_;
    codec.startBitrate = settings.startBitrate_;
    codec.minBitrate = 100;
    codec.maxBitrate = 10000;//settings.maxBitrate_;
    codec.targetBitrate = settings.startBitrate_;
    codec.width = settings.encodeWidth_;
    codec.height = settings.encodeHeight_;
    
    return codec;
}

//******************************************************************************
FrameScaler::FrameScaler(unsigned int dstWidth, unsigned int dstHeight):
srcWidth_(0), srcHeight_(0), 
dstWidth_(dstWidth), dstHeight_(dstHeight)
{
    initScaledFrame();
}

const WebRtcVideoFrame&
FrameScaler::operator()(const WebRtcVideoFrame& frame)
{
    if (srcWidth_ != frame.width() || srcHeight_ != frame.height())
    {
        srcWidth_ = frame.width(); srcHeight_ = frame.height();
        scaler_.Set(srcWidth_, srcHeight_, dstWidth_, dstHeight_,
            webrtc::kI420, webrtc::kI420, webrtc::kScaleBilinear);
    }

    int res = scaler_.Scale(frame, &scaledFrame_);
    return (res == 0 ? scaledFrame_ : frame);
}

void FrameScaler::initScaledFrame()
{
    int stride_y = dstWidth_;
    int stride_uv = (dstWidth_ + 1) / 2;
    int target_width = dstWidth_;
    int target_height = dstHeight_;
    
    
    int ret = scaledFrame_.CreateEmptyFrame(target_width,
                                            abs(target_height),
                                            stride_y,
                                            stride_uv, stride_uv);
    
    if (ret < 0) throw std::runtime_error("failed to allocate scaled frame");
}

//********************************************************************************
#pragma mark - construction/destruction
VideoCoder::VideoCoder(const VideoCoderParams& coderParams, IEncoderDelegate* delegate,
    KeyEnforcement keyEnforcement) :
NdnRtcComponent(),
coderParams_(coderParams),
delegate_(delegate),
gopCounter_(0),
codec_(VideoCoder::codecFromSettings(coderParams_)),
codecSpecificInfo_(nullptr),
keyEnforcement_(keyEnforcement),
#ifdef USE_VP9
    encoder_(VP9Encoder::Create())
#else
    encoder_(VP8Encoder::Create())
#endif
{
    assert(delegate_);
    description_ = "coder";
    keyFrameType_.push_back(webrtc::kKeyFrame);

    if (!encoder_.get())
        throw std::runtime_error("Error creating encoder");

    encoder_->RegisterEncodeCompleteCallback(this);
    int maxPayload = 1440;

    if (encoder_->InitEncode(&codec_, boost::thread::hardware_concurrency(), maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        throw std::runtime_error("Can't initialize encoder");

    LogInfoC
    << "initialized. max payload " << maxPayload
    << " parameters: " << plotCodec(codec_) << endl;
}

VideoCoder::~VideoCoder()
{}

//********************************************************************************
#pragma mark - public
void VideoCoder::onRawFrame(const WebRtcVideoFrame &frame)
{
    LogTraceC << "encoding..." << endl;

    if (frame.width() != coderParams_.encodeWidth_ || 
            frame.height() != coderParams_.encodeHeight_)
        throw std::runtime_error("Frame size does not equal encoding resolution");

    encodeComplete_ = false;
    delegate_->onEncodingStarted();
    
    int err;
    if (gopCounter_%coderParams_.gop_ == 0)
    {
        err = encoder_->Encode(frame, codecSpecificInfo_, &keyFrameType_);
        if (keyEnforcement_ == KeyEnforcement::EncoderDefined) gopCounter_ = 1;
    }
    else
        err = encoder_->Encode(frame, codecSpecificInfo_, NULL);
    
    if (keyEnforcement_ == KeyEnforcement::Timed) gopCounter_++;

    if (!encodeComplete_)
    {
        LogTraceC << "frame dropped" << endl;
        delegate_->onDroppedFrame();
    }

    LogTraceC << "encode result " << err << endl;
    
    if (err != WEBRTC_VIDEO_CODEC_OK)
        LogErrorC <<"can't encode frame due to error " << err << std::endl;
}

//********************************************************************************
#pragma mark - interfaces realization - EncodedImageCallback
int32_t VideoCoder::Encoded(const webrtc::EncodedImage& encodedImage,
                            const webrtc::CodecSpecificInfo* codecSpecificInfo,
                            const webrtc::RTPFragmentationHeader* fragmentation)
{
    encodeComplete_ = true;
    if (keyEnforcement_ == KeyEnforcement::Gop) gopCounter_++;

    /*
    LogInfoC << "encoded"
    << " type " << ((encodedImage._frameType == webrtc::kKeyFrame) ? "KEY":((encodedImage._frameType == webrtc::kSkipFrame)?"SKIP":"DELTA"))
    << " complete " << encodedImage._completeFrame << "\n"
    << " has Received SLI " << codecSpecificInfo->codecSpecific.VP8.hasReceivedSLI
    << " picture Id SLI " << (int)codecSpecificInfo->codecSpecific.VP8.pictureIdSLI
    << " has Received RPSI " << codecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI
    << " picture Id RPSI " << codecSpecificInfo->codecSpecific.VP8.pictureIdRPSI
    << " picture Id " << codecSpecificInfo->codecSpecific.VP8.pictureId
    << " non Reference " << codecSpecificInfo->codecSpecific.VP8.nonReference
    << " temporalIdx " << (int)codecSpecificInfo->codecSpecific.VP8.temporalIdx
    << " layerSync " << codecSpecificInfo->codecSpecific.VP8.layerSync
    << " tl0PicIdx " << codecSpecificInfo->codecSpecific.VP8.tl0PicIdx
    << " keyIdx " << (int)codecSpecificInfo->codecSpecific.VP8.keyIdx
    << std::endl; 
    */
    delegate_->onEncodedFrame(encodedImage);
    return 0;
}
