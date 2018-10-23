// 
// audio-playout-impl.hpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __audio_playout_impl_h__
#define __audio_playout_impl_h__

#include "playout-impl.hpp"
#include "ndnrtc-common.hpp"
#include "frame-buffer.hpp"
#include "frame-buffer.hpp"
#include "webrtc-audio-channel.hpp"

namespace ndnrtc {
  class AudioRenderer;
  
  class AudioPlayoutImpl : public PlayoutImpl
  {
  	typedef statistics::StatisticsStorage StatStorage;
  public:
    AudioPlayoutImpl(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage = 
            boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()),
            const WebrtcAudioChannel::Codec& codec = WebrtcAudioChannel::Codec::G722,
            unsigned int deviceIdx = 0);
    ~AudioPlayoutImpl(){}

    void start(unsigned int fastForwardMs = 0);
    void stop();

    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    AudioBundleSlot bundleSlot_;
    PacketNumber packetCount_;
    boost::shared_ptr<AudioRenderer> renderer_;

    bool
    processSample(const boost::shared_ptr<const BufferSlot>&);
  };
}


#endif