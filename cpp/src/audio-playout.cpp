//
//  audio-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "audio-playout.h"
#include "audio-playout-impl.h"

using namespace ndnrtc;

AudioPlayout::AudioPlayout(boost::asio::io_service& io,
    const boost::shared_ptr<IPlaybackQueue>& queue,
    const boost::shared_ptr<StatStorage>& statStorage):
Playout(boost::make_shared<AudioPlayoutImpl>(io, queue, statStorage))
{}

void AudioPlayout::start(unsigned int devIdx, WebrtcAudioChannel::Codec codec)
{ 
    pimpl()->start(devIdx, codec); 
}

void AudioPlayout::stop() 
{ 
    pimpl()->stop(); 
}

AudioPlayoutImpl* AudioPlayout::pimpl() 
{ 
    return (AudioPlayoutImpl*)Playout::pimpl(); 
}
