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
using namespace ndnrtc::new_api;
using namespace webrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoDecoder::NdnVideoDecoder() :
frameConsumer_(NULL)
{
    memset(&codec_, 0, sizeof(codec_));
}

//********************************************************************************
#pragma mark - public
int NdnVideoDecoder::init(const VideoCoderParams& settings)
{
    int res = RESULT_OK;
    
    settings_ = settings;
    res = VideoCoder::getCodecFromSetings(settings_, codec_);
    description_ = NdnRtcUtils::toString("decoder-%d", codec_.startBitrate);
    
    if (RESULT_GOOD(res))
        return resetDecoder();
    
    return 0;
}

void NdnVideoDecoder::reset()
{
    if (frameCount_ > 0)
    {
        LogTraceC << "resetting decoder..." << std::endl;
        int res = decoder_->Release();
        resetDecoder();
        LogTraceC << "decoder reset" << std::endl;
    }
}

//********************************************************************************
#pragma mark - intefaces realization webrtc::DecodedImageCallback
int32_t NdnVideoDecoder::Decoded(WebRtcVideoFrame &decodedImage)
{
    decodedImage.set_render_time_ms(NdnRtcUtils::millisecondTimestamp());
    
    if (frameConsumer_)
        frameConsumer_->onDeliverFrame(decodedImage, capturedTimestamp_);
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization IEncodedFrameConsumer
void NdnVideoDecoder::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                              double timestamp,
                                              bool completeFrame)
{
    LogTraceC
    << " type " << NdnRtcUtils::stringFromFrameType(encodedImage._frameType)
    << " complete (encoder) " << encodedImage._completeFrame
    << " complete (ndn) " << completeFrame
    << std::endl;
    
    if (frameCount_ == 0)
        LogInfoC << "start decode from "
        << (encodedImage._frameType == webrtc::kKeyFrame ? "KEY" : "DELTA")
        << std::endl;
    
    capturedTimestamp_ = timestamp;
    frameCount_++;
    
    if (decoder_->Decode(encodedImage, !completeFrame, NULL) != WEBRTC_VIDEO_CODEC_OK)
    {
        LogTraceC << "error decoding " << endl;
        
        notifyError(-1, "can't decode frame");
    }
    else
    {
        LogTraceC << "decoded" << endl;
    }
}

int NdnVideoDecoder::resetDecoder()
{
    frameCount_ = 0;
    
#ifdef USE_VP9
    decoder_.reset(VP9Decoder::Create());
#else
    decoder_.reset(VP8Decoder::Create());
#endif
    
    if (!decoder_.get())
        return notifyError(RESULT_ERR, "can't create decoder");
    
    decoder_->RegisterDecodeCompleteCallback(this);
    
    if (decoder_->InitDecode(&codec_, 1) != WEBRTC_VIDEO_CODEC_OK)
        return notifyError(RESULT_ERR, "can't initialize decoder");
    
    return RESULT_OK;
}