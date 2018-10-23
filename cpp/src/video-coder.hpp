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

#include <webrtc/modules/video_coding/include/video_codec_interface.h>

#include "webrtc.hpp"
#include "ndnrtc-common.hpp"
#include "statistics.hpp"
#include "ndnrtc-object.hpp"

#define USE_VP9

namespace ndnrtc
{
class IRawFrameConsumer
{
  public:
    virtual void onRawFrame(const WebRtcVideoFrame &frame) = 0;
};

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
    const WebRtcVideoFrame operator()(const WebRtcVideoFrame &frame);

  private:
    FrameScaler(const FrameScaler &) = delete;

    unsigned int srcWidth_, srcHeight_;
    unsigned int dstWidth_, dstHeight_;
    // webrtc::Scaler scaler_;
    // WebRtcVideoFrame scaledFrame_;
    WebRtcSmartPtr<WebRtcVideoFrameBuffer> scaledFrameBuffer_;

    void
    initScaledFrame();
};

/**
     * This class is a main wrapper for VP8 WebRTC encoder. It consumes raw
     * frames, encodes them using VP8 encoder, configured for specified
     * parameters and passes encoded frames to its' frame consumer class.
     */
class VideoCoder : public NdnRtcComponent,
                   public IRawFrameConsumer,
                   public webrtc::EncodedImageCallback
{
  public:
    enum class KeyEnforcement
    {
        Gop,           // enforce Key frame after GOP amount of encoded frames
        Timed,         // enforce Key frame after GOP amount of raw frames passed to VideoCoder
        EncoderDefined // encoder determines when to insert Key frames (default)
    };

    VideoCoder(const VideoCoderParams &coderParams, IEncoderDelegate *delegate,
               KeyEnforcement = KeyEnforcement::EncoderDefined);

    void onRawFrame(const WebRtcVideoFrame &frame);
    int getGopCounter() const { return gopPos_; }

    static webrtc::VideoCodec codecFromSettings(const VideoCoderParams &settings);

  private:
    VideoCoder(const VideoCoder &) = delete;

    VideoCoderParams coderParams_;
    IEncoderDelegate *delegate_;
    bool encodeComplete_;

    webrtc::VideoCodec codec_;
    const webrtc::CodecSpecificInfo *codecSpecificInfo_;
    std::vector<WebRtcVideoFrameType> keyFrameType_;
    boost::shared_ptr<webrtc::VideoEncoder> encoder_;

    int keyFrameTrigger_, gopPos_;
    KeyEnforcement keyEnforcement_;

    // interface webrtc::EncodedImageCallback
    webrtc::EncodedImageCallback::Result OnEncodedImage(const webrtc::EncodedImage &encoded_image,
                                                        const webrtc::CodecSpecificInfo *codec_specific_info,
                                                        const webrtc::RTPFragmentationHeader *fragmentation);

    void OnDroppedFrame() { /* TODO: implement invoking delegate method here */}
};
}

#endif /* defined(__ndnrtc__video_coder__) */
