//
//  video-thread.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_thread__
#define __ndnrtc__video_thread__

#include <boost/shared_ptr.hpp>

#include "video-coder.hpp"

namespace ndnrtc
{
struct Mutable;
template <typename T>
class VideoFramePacketT;

class VideoThread : public NdnRtcComponent,
                    public IEncoderDelegate
{
  public:
    VideoThread(const VideoCoderParams &coderParams);
    ~VideoThread();

    boost::shared_ptr<VideoFramePacketT<Mutable>> encode(const WebRtcVideoFrame &frame);

    void
        setLogger(boost::shared_ptr<ndnlog::new_api::Logger>);

    unsigned int
    getEncodedNum() { return nEncoded_; }

    unsigned int
    getDroppedNum() { return nDropped_; }

    void
    setDescription(const std::string &desc);

    const VideoCoder &
    getCoder() const { return coder_; }

  private:
    VideoThread(const VideoThread &) = delete;
    VideoCoder coder_;
    unsigned int nEncoded_, nDropped_;

#warning using shared pointer here as libstdc++ on OSX does not support std::move
    // TODO: update code to use std::move on Ubuntu
    boost::shared_ptr<VideoFramePacketT<Mutable>> videoFramePacket_;

    void
    onEncodingStarted();

    void
    onEncodedFrame(const webrtc::EncodedImage &encodedImage);

    void
    onDroppedFrame();
};
}

#endif /* defined(__ndnrtc__video_thread__) */
