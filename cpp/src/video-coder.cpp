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

#undef DEBUG

#include "video-coder.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;

//********************************************************************************
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
int VideoCoder::getCodecFromSetings(const VideoCoderParams &settings,
                                    webrtc::VideoCodec &codec)
{
    // setup default params first
    if (!webrtc::VCMCodecDataBase::Codec(VCM_VP8_IDX, &codec))
    {
        strncpy(codec.plName, "VP8", 31);
        codec.maxFramerate = 30;
        codec.startBitrate  = 300;
        codec.maxBitrate = 4000;
        codec.width = 640;
        codec.height = 480;
        codec.plType = VCM_VP8_PAYLOAD_TYPE;
        codec.qpMax = 56;
        codec.codecType = webrtc::kVideoCodecVP8;
        codec.codecSpecific.VP8.denoisingOn = true;
        codec.codecSpecific.VP8.complexity = webrtc::kComplexityNormal;
        codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
        codec.codecSpecific.VP8.keyFrameInterval = 1000;
    }
    
    // dropping frames
    codec.codecSpecific.VP8.frameDroppingOn = settings.dropFramesOn_;
    
    // customize parameteres if possible
    int res = RESULT_OK;
    codec.maxFramerate = (int)settings.codecFrameRate_;
    codec.startBitrate = settings.startBitrate_;
    codec.minBitrate = 100;
    codec.maxBitrate = settings.maxBitrate_;
    codec.targetBitrate = settings.startBitrate_;
    codec.width = settings.encodeWidth_;
    codec.height = settings.encodeHeight_;
    
    return res;
}

//********************************************************************************
#pragma mark - construction/destruction
VideoCoder::VideoCoder() :
NdnRtcComponent(),
frameConsumer_(nullptr)
{
    rateMeter_ = NdnRtcUtils::setupFrequencyMeter(4);
    description_ = "coder";
    memset(&codec_, 0, sizeof(codec_));
}

VideoCoder::~VideoCoder()
{
    NdnRtcUtils::releaseFrequencyMeter(rateMeter_);
}

//********************************************************************************
#pragma mark - public
int VideoCoder::init(const VideoCoderParams& settings)
{
    settings_ = settings;
    
    if (RESULT_FAIL(VideoCoder::getCodecFromSetings(settings, codec_)))
        notifyError(-1, "some codec parameters were out of bounds. \
                    actual parameters: %s", plotCodec(codec_));
    
    encoder_.reset(VP8Encoder::Create());
    
    keyFrameType_.clear();
    keyFrameType_.push_back(webrtc::kKeyFrame);
    
    if (!encoder_.get())
        return notifyError(-1, "can't create VP8 encoder");
    
    encoder_->RegisterEncodeCompleteCallback(this);
    
#warning how payload can be changed?
    int maxPayload = 1440;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        return notifyError(-1, "can't initialize VP8 encoder");
    
    LogInfoC
    << "initialized. max payload " << maxPayload
    << " parameters: " << plotCodec(codec_) << endl;
    
    initScaledFrame();
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - EncodedImageCallback
int32_t VideoCoder::Encoded(const webrtc::EncodedImage& encodedImage,
                            const webrtc::CodecSpecificInfo* codecSpecificInfo,
                            const webrtc::RTPFragmentationHeader* fragmentation)
{
    LogTraceC << "encoding time "
    << NdnRtcUtils::microsecondTimestamp() - startEncoding_
    << " usec" << std::endl;
    
    counter_++;
    
    settings_.codecFrameRate_ = NdnRtcUtils::currentFrequencyMeterValue(rateMeter_);
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage, deliveredTimestamp_);
    
#warning need to handle return value
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void VideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame,
                                   double timestamp)
{
    LogTraceC << "encoding..." << endl;
    startEncoding_ = NdnRtcUtils::microsecondTimestamp();
    
    if (counter_ %2 == 0)
    {
        LogTraceC << "previous frame was dropped" << endl;
        nDroppedByEncoder_++;
        counter_++;
    }
    
    counter_++;
    deliveredTimestamp_  = timestamp;
    
    int err;
    bool scaled = false;
    
    if (frame.width() != settings_.encodeWidth_ ||
        frame.height() != settings_.encodeHeight_)
    {
        frameScaler_.Set(frame.width(),frame.height(),
                         settings_.encodeWidth_, settings_.encodeHeight_,
                         webrtc::kI420, webrtc::kI420,
                         webrtc::kScaleBilinear);
        int res = frameScaler_.Scale(frame, &scaledFrame_);
        
        if (res != 0)
            notifyError(-1, "couldn't scale frame");
        else
            scaled = true;
    }
    
    I420VideoFrame &processedFrame = (scaled)?scaledFrame_:frame;
    
    if (keyFrameCounter_%settings_.gop_ == 0)
        err = encoder_->Encode(processedFrame, NULL, &keyFrameType_);
    else
        err = encoder_->Encode(processedFrame, NULL, NULL);
    
    keyFrameCounter_++;
    
    LogTraceC << "encode result " << err << endl;
    
    if (err != WEBRTC_VIDEO_CODEC_OK)
        notifyError(-1, "can't encode frame due to error %d",err);
}

void VideoCoder::initScaledFrame()
{
    int stride_y = settings_.encodeWidth_;
    int stride_uv = (settings_.encodeWidth_ + 1) / 2;
    int target_width = settings_.encodeWidth_;
    int target_height = settings_.encodeHeight_;
    
    
    int ret = scaledFrame_.CreateEmptyFrame(target_width,
                                            abs(target_height),
                                            stride_y,
                                            stride_uv, stride_uv);
    
    if (ret < 0)
        notifyError(RESULT_ERR, "failed to allocate scaled frame");
}
