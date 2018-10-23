//
//  video-coder.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

// #define USE_VP9

#include <boost/thread.hpp>
#include <webrtc/modules/video_coding/codecs/vp8/include/vp8.h>
#include <webrtc/modules/video_coding/codecs/vp9/include/vp9.h>
#include <webrtc/modules/video_coding/include/video_coding.h>
#include <webrtc/modules/video_coding/include/video_codec_interface.h>
#include <webrtc/modules/video_coding/codec_database.h>

#include "video-coder.hpp"
#include "threading-capability.hpp"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

// this class is needed to synchronously dispatch all
// frame scaling (see - FrameScaler) calls on the same thread.
// this is WebRTC requirement - all frame scaling calls must be
// executed on the same thread throughout lifecycle
class ScalerThread : public ThreadingCapability
{
  public:
    static ScalerThread *getSharedInstance()
    {
        static ScalerThread scalerThread;
        return &scalerThread;
    }

    void perform(boost::function<void(void)> block)
    {
        performOnMyThread(block);
    }

  private:
    ScalerThread() { startMyThread(); }
    ~ScalerThread() { stopMyThread(); }
};

//********************************************************************************
char *plotCodec(webrtc::VideoCodec codec)
{
    static char msg[256];

    sprintf(msg, "\t\tMax Framerate:\t%d\n \
            \tStart Bitrate:\t%d\n \
            \tMin Bitrate:\t%d\n \
            \tMax Bitrate:\t%d\n \
            \tTarget Bitrate:\t%d\n \
            \tWidth:\t%d\n \
            \tHeight:\t%d",
            codec.maxFramerate,
            codec.startBitrate,
            codec.minBitrate,
            codec.maxBitrate,
            codec.targetBitrate,
            codec.width,
            codec.height);

    return msg;
}

//******************************************************************************
#pragma mark - static
webrtc::VideoCodec VideoCoder::codecFromSettings(const VideoCoderParams &settings)
{
    webrtc::VideoCodec codec;

    // setup default params first
#ifdef USE_VP9
    webrtc::VCMCodecDataBase::Codec(webrtc::kVideoCodecVP9, &codec);
#else
    webrtc::VCMCodecDataBase::Codec(webrtc::kVideoCodecVP8, &codec);
#endif

    // dropping frames
#ifdef USE_VP9
    codec.VP9()->resilience = 1;
    codec.VP9()->frameDroppingOn = settings.dropFramesOn_;
    codec.VP9()->keyFrameInterval = settings.gop_;
#else
    codec.VP8()->resilience = kResilientStream;
    codec.VP8()->frameDroppingOn = settings.dropFramesOn_;
    codec.VP8()->keyFrameInterval = settings.gop_;
#endif

    // customize parameteres if possible
    codec.maxFramerate = (int)settings.codecFrameRate_;
    codec.startBitrate = settings.startBitrate_;
    codec.minBitrate = 100;
    codec.maxBitrate = settings.maxBitrate_;
    codec.targetBitrate = settings.startBitrate_;
    codec.width = settings.encodeWidth_;
    codec.height = settings.encodeHeight_;

    return codec;
}

//******************************************************************************
FrameScaler::FrameScaler(unsigned int dstWidth, unsigned int dstHeight)
    : srcWidth_(0), srcHeight_(0),
      dstWidth_(dstWidth), dstHeight_(dstHeight)
{
    initScaledFrame();
}

const WebRtcVideoFrame
FrameScaler::operator()(const WebRtcVideoFrame &frame)
{
    // try do scaling on this thread. if throws, do it old-fashioned way
    // ScalerThread::getSharedInstance()->perform([&res, &frame, this](){
    //     scaledFrameBuffer_->ScaleFrom(frame);
    // });

    scaledFrameBuffer_->ScaleFrom(*(frame.video_frame_buffer()));

    return WebRtcVideoFrame(scaledFrameBuffer_, frame.rotation(), frame.timestamp_us());
}

void FrameScaler::initScaledFrame()
{
    int stride_y = dstWidth_;
    int stride_uv = (dstWidth_ + 1) / 2;
    int target_width = dstWidth_;
    int target_height = dstHeight_;

    scaledFrameBuffer_ = I420Buffer::Create(target_width,
                                            abs(target_height),
                                            stride_y,
                                            stride_uv, stride_uv);

    if (!scaledFrameBuffer_)
        throw std::runtime_error("failed to allocate scaled frame");
}

//********************************************************************************
#pragma mark - construction/destruction
VideoCoder::VideoCoder(const VideoCoderParams &coderParams, IEncoderDelegate *delegate,
                       KeyEnforcement keyEnforcement)
    : NdnRtcComponent(),
      coderParams_(coderParams),
      delegate_(delegate),
      keyFrameTrigger_(0),
      gopPos_(0),
      codec_(VideoCoder::codecFromSettings(coderParams_)),
      codecSpecificInfo_(nullptr),
      keyEnforcement_(keyEnforcement),
#ifdef USE_VP9
      encoder_(VP9Encoder::Create())
#else
      encoder_(VP8Encoder::Create())
#endif
{
    assert(delegate_);
    description_ = "coder";
    keyFrameType_.push_back(webrtc::kVideoFrameKey);

    if (!encoder_.get())
        throw std::runtime_error("Error creating encoder");

    encoder_->RegisterEncodeCompleteCallback(this);
    int maxPayload = 1440;

    if (encoder_->InitEncode(&codec_, boost::thread::hardware_concurrency(), maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        throw std::runtime_error("Can't initialize encoder");

    LogInfoC
        << "initialized. max payload " << maxPayload
        << " parameters: " << plotCodec(codec_) << endl;
}

//********************************************************************************
#pragma mark - public
void VideoCoder::onRawFrame(const WebRtcVideoFrame &frame)
{
    if (frame.width() != coderParams_.encodeWidth_ ||
        frame.height() != coderParams_.encodeHeight_)
    {
        std::stringstream ss;
        ss << "Frame size (" << frame.width() << "x" << frame.height()
           << ") does not equal encoding resolution ("
           << coderParams_.encodeWidth_ << "x" << coderParams_.encodeHeight_
           << ")" << std::endl;
        throw std::runtime_error(ss.str());
    }

    encodeComplete_ = false;
    delegate_->onEncodingStarted();

    int err;
    if (keyFrameTrigger_ % coderParams_.gop_ == 0)
    {
        gopPos_ = 0;

        LogTraceC << "⤹ encoding ○ (K) " << gopPos_ << endl;
        err = encoder_->Encode(frame, codecSpecificInfo_, &keyFrameType_);
        if (keyEnforcement_ == KeyEnforcement::EncoderDefined)
            keyFrameTrigger_ = 1;
    }
    else
    {
        gopPos_++;

        LogTraceC << "⤹ encoding ○ ? " << gopPos_ << endl;
        err = encoder_->Encode(frame, codecSpecificInfo_, NULL);
    }

    if (!encodeComplete_)
    {
        LogTraceC << " dropped ✕ " << gopPos_ << endl;

        gopPos_--; // if frame was dropped - we decrement gopPos_ back to previous value
        delegate_->onDroppedFrame();
    }

    if (keyEnforcement_ == KeyEnforcement::Timed)
        keyFrameTrigger_++;

    if (err != WEBRTC_VIDEO_CODEC_OK)
        LogErrorC << "can't encode frame due to error " << err << std::endl;
}

//********************************************************************************
#pragma mark - interfaces realization - EncodedImageCallback
webrtc::EncodedImageCallback::Result
VideoCoder::OnEncodedImage(const EncodedImage &encodedImage,
                           const CodecSpecificInfo *codecSpecificInfo,
                           const RTPFragmentationHeader *fragmentation)
{
    encodeComplete_ = true;

    if (encodedImage._frameType == webrtc::kVideoFrameKey)
        gopPos_ = 0;

    LogTraceC << "⤷ encoded  ● "
              << (encodedImage._frameType == webrtc::kVideoFrameKey ? "K " : "D ")
              << gopPos_ << std::endl;

    if (keyEnforcement_ == KeyEnforcement::Gop)
        keyFrameTrigger_++;

    /*
    LogInfoC << "encoded"
    << " type " << ((encodedImage._frameType == webrtc::kKeyFrame) ? "KEY":((encodedImage._frameType == webrtc::kSkipFrame)?"SKIP":"DELTA"))
    << " complete " << encodedImage._completeFrame << "\n"
    << " has Received SLI " << codecSpecificInfo->codecSpecific.VP8.hasReceivedSLI
    << " picture Id SLI " << (int)codecSpecificInfo->codecSpecific.VP8.pictureIdSLI
    << " has Received RPSI " << codecSpecificInfo->codecSpecific.VP8.hasReceivedRPSI
    << " picture Id RPSI " << codecSpecificInfo->codecSpecific.VP8.pictureIdRPSI
    << " picture Id " << codecSpecificInfo->codecSpecific.VP8.pictureId
    << " non Reference " << codecSpecificInfo->codecSpecific.VP8.nonReference
    << " temporalIdx " << (int)codecSpecificInfo->codecSpecific.VP8.temporalIdx
    << " layerSync " << codecSpecificInfo->codecSpecific.VP8.layerSync
    << " tl0PicIdx " << codecSpecificInfo->codecSpecific.VP8.tl0PicIdx
    << " keyIdx " << (int)codecSpecificInfo->codecSpecific.VP8.keyIdx
    << std::endl; 
    */
    delegate_->onEncodedFrame(encodedImage);
    return Result(Result::OK);
}
