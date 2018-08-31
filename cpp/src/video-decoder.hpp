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

#include <webrtc/modules/video_coding/include/video_codec_interface.h>

#include "ndnrtc-common.hpp"
#include "webrtc.hpp"
#include "video-playout-impl.hpp"
#include "interfaces.hpp"

namespace ndnrtc {
    typedef boost::function<void(const FrameInfo&, const WebRtcVideoFrame&)> OnDecodedImage;

    class VideoDecoder : public IEncodedFrameConsumer,
                         public webrtc::DecodedImageCallback,
                         public NdnRtcComponent
    {
    public:
        VideoDecoder(const VideoCoderParams& settings, 
            OnDecodedImage onDecodedImage);
        
        // interface conformance - IEncodedFrameConsumer
        void processFrame(const FrameInfo&, const webrtc::EncodedImage&);

    private:
        VideoCoderParams settings_;
        OnDecodedImage onDecodedImage_;
        webrtc::VideoCodec codec_;
        boost::shared_ptr<webrtc::VideoDecoder> decoder_;
        int frameCount_;
        FrameInfo frameInfo_;
        
        void
        resetDecoder();
        
        // interface conformance - webrtc::DecodedImageCallback
        int32_t Decoded(WebRtcVideoFrame& decodedImage);
    };
}

#endif /* defined(__ndnrtc__video_decoder__) */
