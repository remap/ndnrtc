//
//  video-playout.h
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __video_playout_h__
#define __video_playout_h__

#include "ndnrtc-common.hpp"
#include "playout.hpp"
#include "frame-buffer.hpp"

namespace ndnrtc {
    class IEncodedFrameConsumer;
    class IVideoPlayoutObserver;
    class VideoPlayoutImpl;

    class VideoPlayout : public Playout
    {
    public:
        VideoPlayout(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage = 
                boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));
        
        void stop();
        void registerFrameConsumer(IEncodedFrameConsumer* frameConsumer);
        void deregisterFrameConsumer();

        void attach(IVideoPlayoutObserver* observer);
        void detach(IVideoPlayoutObserver* observer);
        
    private:
        VideoPlayoutImpl* pimpl();
    };
}

#endif
