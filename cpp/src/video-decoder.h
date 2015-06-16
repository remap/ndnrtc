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
#include "base-capturer.h"
#include "video-coder.h"

namespace ndnrtc {
    namespace new_api {
        
        class NdnVideoDecoder : public IEncodedFrameConsumer,
                                public webrtc::DecodedImageCallback,
                                public NdnRtcComponent
        {
        public:
            NdnVideoDecoder();
            ~NdnVideoDecoder() { }
            
            void setFrameConsumer(IRawFrameConsumer *frameConsumer)
            { frameConsumer_ = frameConsumer; };
            
            int init(const VideoCoderParams& settings);
            void reset();
            
            // interface conformance - IEncodedFrameConsumer
            void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                         double timestamp,
                                         bool completeFrame);
        private:
            VideoCoderParams settings_;
            IRawFrameConsumer *frameConsumer_;
            webrtc::VideoCodec codec_;
            boost::shared_ptr<webrtc::VideoDecoder> decoder_;
            double capturedTimestamp_ = 0;
            int frameCount_;
            
            int
            resetDecoder();
            
            // interface conformance - webrtc::DecodedImageCallback
            int32_t Decoded(WebRtcVideoFrame& decodedImage);
        };
    }
}

#endif /* defined(__ndnrtc__video_decoder__) */
