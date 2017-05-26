//
//  audio-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "audio-playout.hpp"
#include "audio-playout-impl.hpp"

using namespace ndnrtc;

AudioPlayout::AudioPlayout(boost::asio::io_service& io,
    const boost::shared_ptr<IPlaybackQueue>& queue,
    const boost::shared_ptr<StatStorage>& statStorage,
    const WebrtcAudioChannel::Codec& codec,
    unsigned int deviceIdx):
Playout(boost::make_shared<AudioPlayoutImpl>(io, queue, statStorage, codec, deviceIdx))
{}

void AudioPlayout::start(unsigned int fastForwardMs)
{ 
    pimpl()->start(fastForwardMs);
}

void AudioPlayout::stop() 
{ 
    pimpl()->stop(); 
}

AudioPlayoutImpl* AudioPlayout::pimpl() 
{ 
    return (AudioPlayoutImpl*)Playout::pimpl(); 
}
