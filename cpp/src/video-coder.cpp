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
            \tMax Bitrate:\t%d\n \
            \tWidth:\t%d\n \
            \tHeight:\t%d",
            codec.maxFramerate,
            codec.startBitrate,
            codec.maxBitrate,
            codec.width,
            codec.height);
    
    return msg;
}

//******************************************************************************
#pragma mark - static
int NdnVideoCoder::getCodec(const ParamsStruct &params, VideoCodec &codec)
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
    codec.maxFramerate = ParamsStruct::validateLE(params.codecFrameRate,
                                                  MaxFrameRate, res,
                                                  DefaultParams.codecFrameRate);
    codec.startBitrate = ParamsStruct::validateLE(params.startBitrate,
                                                  MaxStartBitrate, res,
                                                  DefaultParams.startBitrate);
    codec.maxBitrate = ParamsStruct::validateLE(params.maxBitrate,
                                                MaxBitrate, res,
                                                DefaultParams.maxBitrate);
    codec.width = ParamsStruct::validateLE(params.encodeWidth,
                                           MaxWidth, res,
                                           DefaultParams.encodeWidth);
    codec.height = ParamsStruct::validateLE(params.encodeHeight,
                                            MaxHeight, res,
                                            DefaultParams.encodeHeight);
    
    return res;
}

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoCoder::NdnVideoCoder(const ParamsStruct &params) : NdnRtcObject(params), frameConsumer_(nullptr)
{
    description_ = "coder";
    memset(&codec_, 0, sizeof(codec_));
}
//********************************************************************************
#pragma mark - public
int NdnVideoCoder::init()
{
    if (RESULT_FAIL(NdnVideoCoder::getCodec(params_, codec_)))
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
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - EncodedImageCallback
int32_t NdnVideoCoder::Encoded(webrtc::EncodedImage& encodedImage,
                               const webrtc::CodecSpecificInfo* codecSpecificInfo,
                               const webrtc::RTPFragmentationHeader* fragmentation)
{
    counter_++;
    
    //    TRACE("got encoded byte length: %d\n"
    //          "info: \thasReceivedSLI: %d\n\tpictureIdSLI: %d\n\thasReceivedRPSI: %d\n\t"
    //          "pictureIdRPSI: %ld\n\tpictureId:%d\n\tnonReference:%d\n\tsimulcastIdx:%d\n\t"
    //          "temporalIdx: %d\n\tlayerSync:%d\n\ttl0PicIdx:%d\n\tkeyIdx:%d\n",
    //          encodedImage._length,
    //          codecSpecificInfo->codecSpecific.VP8.hasReceivedSLI,
    //          codecSpecificInfo->codecSpecific.VP8.pictureIdSLI,
    //          codecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI,
    //          codecSpecificInfo->codecSpecific.VP8.pictureIdRPSI,
    //          codecSpecificInfo->codecSpecific.VP8.pictureId,
    //          codecSpecificInfo->codecSpecific.VP8.nonReference,
    //          codecSpecificInfo->codecSpecific.VP8.simulcastIdx,
    //          codecSpecificInfo->codecSpecific.VP8.temporalIdx,
    //          codecSpecificInfo->codecSpecific.VP8.layerSync,
    //          codecSpecificInfo->codecSpecific.VP8.tl0PicIdx,
    //          codecSpecificInfo->codecSpecific.VP8.keyIdx);
    
    encodedImage._timeStamp = NdnRtcUtils::millisecondTimestamp()/1000;
    encodedImage.capture_time_ms_ = NdnRtcUtils::millisecondTimestamp();
    
    encodeTime_ = NdnRtcUtils::microsecondTimestamp()-encodeTime_;

    LogStatC
    << "encoded\t"
    << counter_-1 << "\t"
    << encodeTime_ << endl;
    
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage);
    
#warning need to handle return value
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnVideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    LogTraceC << "encoding..." << endl;
    
    if (counter_ %2 == 0)
    {
        LogTraceC << "previous frame was dropped" << endl;
        nDroppedByEncoder_++;
        counter_++;
    }
    
    counter_++;
    
    encodeTime_ = NdnRtcUtils::microsecondTimestamp();
    
    int err;
    
    if (!keyFrameCounter_%(currentFrameRate_))
        err = encoder_->Encode(frame, NULL, &keyFrameType_);
    else
        err = encoder_->Encode(frame, NULL, NULL);
    
    keyFrameCounter_ = (keyFrameCounter_+1)%(currentFrameRate_);
    
    LogTraceC << "encode result " << err << endl;
    
    if (err != WEBRTC_VIDEO_CODEC_OK)
        notifyError(-1, "can't encode frame due to error %d",err);
}
