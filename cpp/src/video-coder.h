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

#include "ndnrtc-common.h"
#include "base-capturer.h"
#include "statistics.h"

namespace ndnrtc {
    
    namespace new_api {
        class IEncodedFrameConsumer;
        class IEncoderDelegate;
        
        class VideoCoderStatistics : public ObjectStatistics {
        public:
            unsigned int nDroppedByEncoder;
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
            VideoCoder();
            ~VideoCoder();
            
            void setFrameConsumer(IEncoderDelegate *frameConsumer) {
                frameConsumer_ = frameConsumer;
            }
            
            int init(const VideoCoderParams& settings);
            
            // interface conformance - ndnrtc::IRawFrameConsumer
            void onDeliverFrame(WebRtcVideoFrame &frame,
                                double timestamp);
            
            void
            getStatistics(VideoCoderStatistics& statistics)
            { statistics.nDroppedByEncoder = nDroppedByEncoder_; }

            void
            getSettings(VideoCoderParams& settings)
            { settings = settings_; }
            
            static int getCodecFromSetings(const VideoCoderParams &settings,
                                           webrtc::VideoCodec &codec);
            
        private:
            VideoCoderParams settings_;
            unsigned int counter_ = 1;
            unsigned int nDroppedByEncoder_ = 0;
            unsigned int rateMeter_;
            int keyFrameCounter_ = 0;
            
            double deliveredTimestamp_;
            IEncoderDelegate *frameConsumer_ = NULL;
            
            webrtc::VideoCodec codec_;
            const webrtc::CodecSpecificInfo* codecSpecificInfo_;
            std::vector<WebRtcVideoFrameType> keyFrameType_;
            boost::shared_ptr<webrtc::VideoEncoder> encoder_;
            
            webrtc::Scaler frameScaler_;
            WebRtcVideoFrame scaledFrame_;
            
            // interface conformance - webrtc::EncodedImageCallback
            int32_t Encoded(const webrtc::EncodedImage& encodedImage,
                            const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
                            const webrtc::RTPFragmentationHeader* fragmentation = NULL);
            
            void
            initScaledFrame();
        };
        
        class IEncodedFrameConsumer
        {
        public:
            virtual void
            onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                    double captureTimestamp,
                                    bool completeFrame = true) = 0;
        };
        
        class IEncoderDelegate : public IEncodedFrameConsumer
        {
        public:
            virtual void
            onEncodingStarted() = 0;
            
            virtual void
            onFrameDropped() = 0;
        };
    }
}

#endif /* defined(__ndnrtc__video_coder__) */
