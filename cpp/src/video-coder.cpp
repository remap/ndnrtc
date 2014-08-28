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
int NdnVideoCoder::getCodec(const CodecParams &params, VideoCodec &codec)
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
    codec.codecSpecific.VP8.frameDroppingOn = params.dropFramesOn;
    
    // customize parameteres if possible
    int res = RESULT_OK;
    codec.maxFramerate = (int)params.codecFrameRate;
    codec.startBitrate = ParamsStruct::validateLE(params.startBitrate,
                                                  MaxStartBitrate, res,
                                                  DefaultCodecParams.startBitrate);
    codec.minBitrate = 100;
    codec.maxBitrate = ParamsStruct::validateLE(params.maxBitrate,
                                                MaxBitrate, res,
                                                DefaultCodecParams.maxBitrate);
    codec.targetBitrate = codec.startBitrate;
    codec.width = ParamsStruct::validateLE(params.encodeWidth,
                                           MaxWidth, res,
                                           DefaultCodecParams.encodeWidth);
    codec.height = ParamsStruct::validateLE(params.encodeHeight,
                                            MaxHeight, res,
                                            DefaultCodecParams.encodeHeight);
    
    return res;
}

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoCoder::NdnVideoCoder(const CodecParams &params) : NdnRtcObject(),
codecParams_(params),
frameConsumer_(nullptr)
{
    description_ = "coder";
    memset(&codec_, 0, sizeof(codec_));
}

NdnVideoCoder::~NdnVideoCoder()
{
}

//********************************************************************************
#pragma mark - public
int NdnVideoCoder::init()
{
    if (RESULT_FAIL(NdnVideoCoder::getCodec(codecParams_, codec_)))
        notifyError(-1, "some codec parameters were out of bounds. \
                    actual parameters: %s", plotCodec(codec_));
    
    encoder_.reset(VP8Encoder::Create());
    
    currentFrameRate_ = codec_.maxFramerate;
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
int32_t NdnVideoCoder::Encoded(webrtc::EncodedImage& encodedImage,
                               const webrtc::CodecSpecificInfo* codecSpecificInfo,
                               const webrtc::RTPFragmentationHeader* fragmentation)
{
    counter_++;
    
    encodedImage._timeStamp = NdnRtcUtils::millisecondTimestamp()/1000;
    encodedImage.capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
    
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage, deliveredTimestamp_);
    
#warning need to handle return value
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnVideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame,
                                   double timestamp)
{
    LogTraceC << "encoding..." << endl;
    
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
    
    if (frame.width() != codecParams_.encodeWidth ||
        frame.height() != codecParams_.encodeHeight)
    {
        frameScaler_.Set(frame.width(),frame.height(),
                         codecParams_.encodeWidth, codecParams_.encodeHeight,
                         webrtc::kI420, webrtc::kI420,
                         webrtc::kScaleBilinear);
        int res = frameScaler_.Scale(frame, &scaledFrame_);
        
        if (res != 0)
            notifyError(-1, "couldn't scale frame");
        else
            scaled = true;
    }
    
    I420VideoFrame &processedFrame = (scaled)?scaledFrame_:frame;
    
    if (keyFrameCounter_%codecParams_.gop == 0)
        err = encoder_->Encode(processedFrame, NULL, &keyFrameType_);
    else
        err = encoder_->Encode(processedFrame, NULL, NULL);
    
    keyFrameCounter_++;
    
    LogTraceC << "encode result " << err << endl;
    
    if (err != WEBRTC_VIDEO_CODEC_OK)
        notifyError(-1, "can't encode frame due to error %d",err);
}

void NdnVideoCoder::initScaledFrame()
{
    int stride_y = codecParams_.encodeWidth;
    int stride_uv = (codecParams_.encodeWidth + 1) / 2;
    int target_width = codecParams_.encodeWidth;
    int target_height = codecParams_.encodeHeight;
    

    int ret = scaledFrame_.CreateEmptyFrame(target_width,
                                             abs(target_height),
                                             stride_y,
                                             stride_uv, stride_uv);
    
    if (ret < 0)
        notifyError(RESULT_ERR, "failed to allocate scaled frame");
}
