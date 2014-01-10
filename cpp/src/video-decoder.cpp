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

using namespace ndnrtc;
using namespace webrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoDecoder::NdnVideoDecoder(const ParamsStruct &params) :
NdnRtcObject(params), frameConsumer_(NULL)
{
  memset(&codec_, 0, sizeof(codec_));
}

//********************************************************************************
#pragma mark - public
int NdnVideoDecoder::init()
{
  int res = RESULT_OK;
  
  res = NdnVideoCoder::getCodec(params_, codec_);
  
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
    TRACE("pushing decoded frame further");
    frameConsumer_->onDeliverFrame(decodedImage);
  }
  else
    WARN("frame was decoded but never used");
  
  return 0;
}

//********************************************************************************
#pragma mark - intefaces realization IEncodedFrameConsumer
void NdnVideoDecoder::onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
{
  TRACE("trying to decode frame of size %d (type: %s)", encodedImage._length,
        NdnRtcUtils::stringFromFrameType(encodedImage._frameType).c_str());
  
  if (decoder_->Decode(encodedImage, false, NULL) != WEBRTC_VIDEO_CODEC_OK)
    notifyError(-1, "can't decode frame");
  else
    TRACE("decoded");
}
