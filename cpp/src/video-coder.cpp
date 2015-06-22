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
frameConsumer_(nullptr),
codecSpecificInfo_(nullptr)
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
    
#ifdef USE_VP9
    encoder_.reset(VP9Encoder::Create());
#else
    encoder_.reset(VP8Encoder::Create());
#endif
    
    keyFrameType_.clear();
    keyFrameType_.push_back(webrtc::kKeyFrame);
    
    if (!encoder_.get())
        return notifyError(-1, "can't create encoder");
    
    encoder_->RegisterEncodeCompleteCallback(this);
    
#warning how payload can be changed?
    int maxPayload = 1440;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        return notifyError(-1, "can't initialize encoder");
    
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
    
    counter_++;
    settings_.codecFrameRate_ = NdnRtcUtils::currentFrequencyMeterValue(rateMeter_);
    codecSpecificInfo_ = codecSpecificInfo;

    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage, deliveredTimestamp_);
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void VideoCoder::onDeliverFrame(WebRtcVideoFrame &frame,
                                   double timestamp)
{
    LogTraceC << "encoding..." << endl;

    if (frameConsumer_)
        frameConsumer_->onEncodingStarted();
    
    if (counter_ %2 == 0)
    {
        nDroppedByEncoder_++;
        counter_++;
        
        if (frameConsumer_)
            frameConsumer_->onFrameDropped();
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
    
    WebRtcVideoFrame &processedFrame = (scaled)?scaledFrame_:frame;
    
    if (keyFrameCounter_%settings_.gop_ == 0)
        err = encoder_->Encode(processedFrame, codecSpecificInfo_, &keyFrameType_);
    else
        err = encoder_->Encode(processedFrame, codecSpecificInfo_, NULL);
    
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
