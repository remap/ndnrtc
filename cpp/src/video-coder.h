//
//  video-coder.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_coder__
#define __ndnrtc__video_coder__

#include <webrtc/modules/video_coding/codecs/interface/video_codec_interface.h>
#include <webrtc/common_video/libyuv/include/scaler.h>

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "base-capturer.h"
#include "statistics.h"

//#define USE_VP9

namespace ndnrtc {
    namespace new_api {
        class IEncoderDelegate
        {
        public:
            virtual void
            onEncodingStarted() = 0;
            
            virtual void
            onEncodedFrame(const webrtc::EncodedImage &encodedImage) = 0;

            virtual void
            onDroppedFrame() = 0;
        };

        class VideoCoderStatistics : public ObjectStatistics {
        public:
            unsigned int nDroppedByEncoder;
        };
        
        /**
         * This class performs scaling of raw frames. WebRTC uses frame buffers
         * pool for allocating frames memory. All frame operations that involve 
         * memory manipulations must use buffer pool. According to WebRTC design,
         * access to buffer pool is restricted to only one thread - the one that
         * was the first to access pool. As frame scaling involves pool access,
         * one has to ensure that scaling is always performed on the same thread.
         * This rule affects applies to all FrameScaler instances.
         */
        class FrameScaler 
        {
        public:
            FrameScaler(unsigned int dstWidth, unsigned int dstHeight);
            const WebRtcVideoFrame& operator()(const WebRtcVideoFrame& frame);
        
        private:
            FrameScaler(const FrameScaler&) = delete;

            unsigned int srcWidth_, srcHeight_;
            unsigned int dstWidth_, dstHeight_;
            webrtc::Scaler scaler_;
            WebRtcVideoFrame scaledFrame_;

            void
            initScaledFrame();
        };

        /**
         * This class is a main wrapper for VP8 WebRTC encoder. It consumes raw
         * frames, encodes them using VP8 encoder, configured for specified
         * parameters and passes encoded frames to its' frame consumer class.
         */
        class VideoCoder :  public NdnRtcComponent,
                            public IRawFrameConsumer,
                            public webrtc::EncodedImageCallback
        {
        public:
            enum class KeyEnforcement {
                Gop,            // enforce Key frame after GOP amount of encoded frames
                Timed,          // enforce Key frame after GOP amount of raw frames passed to VideoCoder
                EncoderDefined  // encoder determines when to insert Key frames (default)
            };

            VideoCoder(const VideoCoderParams& coderParams, IEncoderDelegate* delegate, 
                KeyEnforcement = KeyEnforcement::EncoderDefined);
            ~VideoCoder();

            void onRawFrame(const WebRtcVideoFrame &frame);

        private:
            VideoCoder(const VideoCoder&) = delete;

            VideoCoderParams coderParams_;
            IEncoderDelegate *delegate_;
            bool encodeComplete_;
            
            webrtc::VideoCodec codec_;
            const webrtc::CodecSpecificInfo* codecSpecificInfo_;
            std::vector<WebRtcVideoFrameType> keyFrameType_;
            boost::shared_ptr<webrtc::VideoEncoder> encoder_;

            int gopCounter_;
            KeyEnforcement keyEnforcement_;
            
            // interface webrtc::EncodedImageCallback
            int32_t Encoded(const webrtc::EncodedImage& encodedImage,
                            const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
                            const webrtc::RTPFragmentationHeader* fragmentation = NULL);

            static webrtc::VideoCodec codecFromSettings(const VideoCoderParams &settings);
        };
    }
}

#endif /* defined(__ndnrtc__video_coder__) */
