//
// remote-video-stream.hpp
//
//  Created by Peter Gusev on 29 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_video_stream_h__
#define __remote_video_stream_h__

#include "remote-stream-impl.hpp"
#include "sample-validator.hpp"
#include "webrtc.hpp"
#include "interfaces.hpp"

namespace ndnrtc
{
class IPlayoutControl;
class VideoPlayout;
class PipelineControl;
class ManifestValidator;
class VideoDecoder;
class IExternalRenderer;
class IVideoPlayoutObserver;
class IBufferObserver;

class RemoteVideoStreamImpl : public RemoteStreamImpl
{
  public:
    /**
     * Constructor for metadata-bootstrap remote stream (for live playback)
     */
    RemoteVideoStreamImpl(boost::asio::io_service &io,
                          const boost::shared_ptr<ndn::Face> &face,
                          const boost::shared_ptr<ndn::KeyChain> &keyChain,
                          const std::string &streamPrefix);
    /**
     * Constructor for seed-bootstrap remote stream (for historical playback)
     */
    RemoteVideoStreamImpl(boost::asio::io_service &io,
                          const boost::shared_ptr<ndn::Face> &face,
                          const boost::shared_ptr<ndn::KeyChain> &keyChain,
                          const std::string &streamPrefix,
                          const std::string &threadName);
    ~RemoteVideoStreamImpl();

    void start(const std::string &threadName, IExternalRenderer *render);
    void start(const RemoteVideoStream::FetchingRuleSet& ruleset, IExternalRenderer *render);
    void initiateFetching();
    void stopFetching();
    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    bool isPlaybackDriven_;
    boost::shared_ptr<IVideoPlayoutObserver> playbackObserver_;
    boost::shared_ptr<IBufferObserver> bufferObserver_;
    RemoteVideoStream::FetchingRuleSet ruleset_;

    boost::shared_ptr<ManifestValidator> validator_;
    IExternalRenderer *renderer_;
    boost::shared_ptr<VideoDecoder> decoder_;

    void construct();
    void feedFrame(const FrameInfo&, const WebRtcVideoFrame &);
    void setupDecoder();
    void releaseDecoder();
    void setupPipelineControl();
    void releasePipelineControl();
};
}

#endif
