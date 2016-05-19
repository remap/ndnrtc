//
//  audio-playout.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__audio_playout__
#define __ndnrtc__audio_playout__

#include "ndnrtc-common.h"
#include "playout.h"
#include "frame-buffer.h"
#include "webrtc-audio-channel.h"

namespace ndnrtc {
  typedef new_api::statistics::StatisticsStorage StatStorage;
  class BufferSlot;
  class AudioRenderer;
  
  class AudioPlayout : public Playout
  {
  public:
    AudioPlayout(boost::asio::io_service& io,
            const boost::shared_ptr<PlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage = 
              boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));
    ~AudioPlayout(){}

    void start(unsigned int devIdx = 0, 
      WebrtcAudioChannel::Codec codec = WebrtcAudioChannel::Codec::G722);
    void stop();

  private:
    AudioBundleSlot bundleSlot_;
    PacketNumber packetCount_;
    boost::shared_ptr<AudioRenderer> renderer_;

    void
    processSample(const boost::shared_ptr<const BufferSlot>&);
  };
}

#endif /* defined(__ndnrtc__audio_playout__) */
