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

//#define NDN_TRACE

#include "video-coder.h"

using namespace ndnrtc;
using namespace webrtc;

//********************************************************************************
//********************************************************************************
const std::string NdnVideoCoderParams::ParamNameFrameRate = "frameRate";
const std::string NdnVideoCoderParams::ParamNameStartBitRate = "startBitRate";
const std::string NdnVideoCoderParams::ParamNameMaxBitRate = "maxBitRate";
const std::string NdnVideoCoderParams::ParamNameWidth = "encodeWidth";
const std::string NdnVideoCoderParams::ParamNameHeight = "encodeHeight";

char* plotCodec(webrtc::VideoCodec codec)
{
    static char msg[256];
    
    sprintf(msg, "\t\tMax Framerate:\t%d\n \
         \tStart Bitrate:\t%d\n \
         \tMax Bitrate:\t%d\n \
         \tWidth:\t%d\n \
         \tHeight:\t%d\
         \0", codec.maxFramerate, codec.startBitrate, codec.maxBitrate,
         codec.width, codec.height);
    
    return msg;
}


//********************************************************************************
#pragma mark - construction/destruction
NdnVideoCoder::NdnVideoCoder(const NdnParams *params) : NdnRtcObject(params), frameConsumer_(nullptr)
{
    memset(&codec_, 0, sizeof(codec_));
}
//********************************************************************************
#pragma mark - public
int NdnVideoCoder::init()
{
    if (!hasParams())
        notifyErrorNoParams();

    // setup default params first
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
    // customize parameteres if possible
    if (getParams()->getFrameRate((int*)&codec_.maxFramerate) < 0)
        notifyErrorBadArg(NdnVideoCoderParams::ParamNameFrameRate);
    
    if (getParams()->getStartBitRate((int*)&codec_.startBitrate) < 0)
        notifyErrorBadArg(NdnVideoCoderParams::ParamNameStartBitRate);
    
    if (getParams()->getMaxBitRate((int*)&codec_.maxBitrate) < 0)
        notifyErrorBadArg(NdnVideoCoderParams::ParamNameMaxBitRate);
    
    if (getParams()->getWidth((int*)&codec_.width) < 0)
        notifyErrorBadArg(NdnVideoCoderParams::ParamNameWidth);
    
    if (getParams()->getHeight((int*)&codec_.height) < 0)
        notifyErrorBadArg(NdnVideoCoderParams::ParamNameHeight);

    encoder_.reset(VP8Encoder::Create());
    
    if (!encoder_.get())
        return notifyError(-1, "can't create VP8 encoder");
    
    encoder_->RegisterEncodeCompleteCallback(this);
    
#warning how payload can be changed?
    int maxPayload = 3900;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        return notifyError(-1, "can't initialize VP8 encoder");
    
    INFO("Initialized encoder with max payload %d and parameters: \n%s", maxPayload, plotCodec(codec_));
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - EncodedImageCallback
int32_t NdnVideoCoder::Encoded(webrtc::EncodedImage& encodedImage,
                const webrtc::CodecSpecificInfo* codecSpecificInfo,
                const webrtc::RTPFragmentationHeader* fragmentation)
{
    TRACE("got encoded byte length: %d", encodedImage._length);
    
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage);
    
#warning need to handle return value
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnVideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    TRACE("encoding...");
    int err = encoder_->Encode(frame, NULL, NULL);
    
    if (err != WEBRTC_VIDEO_CODEC_OK)
        notifyError(-1, "can't encode frame due to error %d",err);
}

//********************************************************************************
#pragma mark - private

