//
// remote-audio-stream.hpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_audio_stream_h__
#define __remote_audio_stream_h__

#include "remote-stream-impl.hpp"

namespace ndnrtc
{
class SampleValidator;

class RemoteAudioStreamImpl : public RemoteStreamImpl
{
  public:
    RemoteAudioStreamImpl(boost::asio::io_service &io,
                          const std::shared_ptr<ndn::Face> &face,
                          const std::shared_ptr<ndn::KeyChain> &keyChain,
                          const std::string &streamPrefix);
    ~RemoteAudioStreamImpl();

    void initiateFetching();
    void stopFetching();
    void setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    boost::asio::io_service &io_;
    std::shared_ptr<SampleValidator> validator_;

    void setupPlayout();
    void releasePlayout();
    void setupPipelineControl();
    void releasePipelineControl();
};
}

#endif
