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
        return notifyErrorNoParams();
    
    if (getParams()->getFrameRate((int*)&codec_.maxFramerate) < 0)
        return notifyErrorBadArg(NdnVideoCoderParams::ParamNameFrameRate);

    if (getParams()->getStartBitRate((int*)&codec_.startBitrate) < 0)
        return notifyErrorBadArg(NdnVideoCoderParams::ParamNameStartBitRate);
    
    if (getParams()->getMaxBitRate((int*)&codec_.maxBitrate) < 0)
        return notifyErrorBadArg(NdnVideoCoderParams::ParamNameMaxBitRate);
    
    if (getParams()->getWidth((int*)&codec_.width) < 0)
        return notifyErrorBadArg(NdnVideoCoderParams::ParamNameWidth);

    if (getParams()->getHeight((int*)&codec_.height) < 0)
        return notifyErrorBadArg(NdnVideoCoderParams::ParamNameHeight);

#warning what is this parameter?
    codec_.qpMax = 56;
    codec_.codecSpecific.VP8.denoisingOn = true;
    
    encoder_.reset(VP8Encoder::Create());
    
    if (!encoder_.get())
        return notifyError(-1, "can't create VP8 encoder");
    
    encoder_.get()->RegisterEncodeCompleteCallback(this);
    
#warning hoe payload can be changed?
    int maxPayload = 3900;
    
    if (encoder_.get()->InitEncode(&codec_, 2, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        return notifyError(-1, "can't initialize VP8 encoder");
    
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - EncodedImageCallback
int32_t NdnVideoCoder::Encoded(webrtc::EncodedImage& encodedImage,
                const webrtc::CodecSpecificInfo* codecSpecificInfo,
                const webrtc::RTPFragmentationHeader* fragmentation)
{
//    TRACE("got encoded frame: length - %d, size - %d, completed - %d", encodedImage._length, encodedImage._size, encodedImage._completeFrame);
    
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered(encodedImage);
    
#warning need to handle return value
    return 0;
}
//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnVideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    encoder_.get()->Encode(frame, NULL, NULL);
}

//********************************************************************************
#pragma mark - private

