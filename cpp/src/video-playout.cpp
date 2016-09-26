//
//  video-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "video-playout.hpp"
#include "video-playout-impl.hpp"

using namespace ndnrtc;

VideoPlayout::VideoPlayout(boost::asio::io_service& io,
    const boost::shared_ptr<IPlaybackQueue>& queue,
    const boost::shared_ptr<StatStorage>& statStorage):
Playout(boost::make_shared<VideoPlayoutImpl>(io, queue, statStorage)){}

void VideoPlayout::stop() 
{ pimpl()->stop(); } 

void VideoPlayout::registerFrameConsumer(IEncodedFrameConsumer* frameConsumer)
{ pimpl()->registerFrameConsumer(frameConsumer); }

void VideoPlayout::deregisterFrameConsumer()
{ pimpl()->deregisterFrameConsumer(); }

void VideoPlayout::attach(IVideoPlayoutObserver* observer)
{ pimpl()->attach(observer); }

void VideoPlayout::detach(IVideoPlayoutObserver* observer)
{ pimpl()->detach(observer); }

VideoPlayoutImpl* VideoPlayout::pimpl()
{ return (VideoPlayoutImpl*)Playout::pimpl(); }
