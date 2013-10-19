//
//  video-decoder.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_decoder__
#define __ndnrtc__video_decoder__

#include "ndnrtc-common.h"
#include "webrtc.h"
#include "camera-capturer.h"
#include "video-coder.h"

namespace ndnrtc {
    class NdnVideoDecoder : public NdnRtcObject, public IEncodedFrameConsumer, public webrtc::DecodedImageCallback {
    public:
        // construction/destruction
        NdnVideoDecoder(const NdnParams *coderParams);
        ~NdnVideoDecoder() { }
        
        void setFrameConsumer(IRawFrameConsumer *frameConsumer) { frameConsumer_ = frameConsumer; };
        int init();
        
        // interface conformance - webrtc::DecodedImageCallback
        int32_t Decoded(webrtc::I420VideoFrame& decodedImage);
        // interface conformance - IEncodedFrameConsumer
        void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage);

    private:
        IRawFrameConsumer *frameConsumer_;
        webrtc::VideoCodec codec_;
        shared_ptr<webrtc::VideoDecoder> decoder_;
    };
}

#endif /* defined(__ndnrtc__video_decoder__) */
