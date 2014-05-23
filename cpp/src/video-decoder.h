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
    class NdnVideoDecoder : public IEncodedFrameConsumer,
    public webrtc::DecodedImageCallback, public NdnRtcObject
    {
    public:
        NdnVideoDecoder(const ParamsStruct &coderParams);
        ~NdnVideoDecoder() { }
        
        void setFrameConsumer(IRawFrameConsumer *frameConsumer)
        { frameConsumer_ = frameConsumer; };
        
        int init();
        
        // interface conformance - webrtc::DecodedImageCallback
        int32_t Decoded(webrtc::I420VideoFrame& decodedImage);
        // interface conformance - IEncodedFrameConsumer
        void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                     double timestamp);

    private:
        IRawFrameConsumer *frameConsumer_;
        webrtc::VideoCodec codec_;
        shared_ptr<webrtc::VideoDecoder> decoder_;
        double capturedTimestamp_ = 0;
    };
}

#endif /* defined(__ndnrtc__video_decoder__) */
