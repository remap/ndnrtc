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

#include "video-decoder.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoDecoder::NdnVideoDecoder(const CodecParams &codecParams) :
NdnRtcObject(),
codecParams_(codecParams),
frameConsumer_(NULL)
{
    description_ = NdnRtcUtils::toString("decoder-%d", codecParams_.startBitrate);
    memset(&codec_, 0, sizeof(codec_));
}

//********************************************************************************
#pragma mark - public
int NdnVideoDecoder::init()
{
    int res = RESULT_OK;
    
    res = NdnVideoCoder::getCodec(codecParams_, codec_);
    
    if (RESULT_GOOD(res))
    {
        decoder_.reset(VP8Decoder::Create());
        
        if (!decoder_.get())
            return notifyError(RESULT_ERR, "can't create VP8 decoder");
        
        decoder_->RegisterDecodeCompleteCallback(this);
        
        if (decoder_->InitDecode(&codec_, 1) != WEBRTC_VIDEO_CODEC_OK)
            return notifyError(RESULT_ERR, "can't initialize VP8 decoder");
    }
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization webrtc::DecodedImageCallback
int32_t NdnVideoDecoder::Decoded(I420VideoFrame &decodedImage)
{
    decodedImage.set_render_time_ms(NdnRtcUtils::millisecondTimestamp());
    
    if (frameConsumer_)
    {
        frameConsumer_->onDeliverFrame(decodedImage, capturedTimestamp_);
    }
    else
        LogWarnC << "unused decoded" << endl;
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization IEncodedFrameConsumer
void NdnVideoDecoder::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                              double timestamp)
{
    LogTraceC
    << "to decode " << encodedImage._length
    << " type " << NdnRtcUtils::stringFromFrameType(encodedImage._frameType) << endl;
    capturedTimestamp_ = timestamp;
    
    if (decoder_->Decode(encodedImage, false, NULL) != WEBRTC_VIDEO_CODEC_OK)
    {
        LogTraceC << "error decoding " << endl;
        
        notifyError(-1, "can't decode frame");
    }
    else
    {
        LogTraceC << "decoded" << endl;
    }
}
