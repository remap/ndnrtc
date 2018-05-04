//
//  playout.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#ifndef __playout_h__
#define __playout_h__

#include <boost/asio.hpp>

#include "statistics.hpp"
#include "ndnrtc-object.hpp"

namespace ndnlog {
    namespace new_api {
        class Logger;
    }
}

namespace ndnrtc {
    class PlayoutImpl;
    class IPlaybackQueue;
    class IPlayoutObserver;
    typedef statistics::StatisticsStorage StatStorage;

    class IPlayout {
    public:
        virtual void start(unsigned int fastForwardMs = 0) = 0;
        virtual void stop() = 0;
        virtual bool isRunning() const = 0;
        virtual void addAdjustment(int64_t) = 0;
    };

    /**
     * Playout is a base class for media streams (audio and video) playout 
     * mechanism. It implements functionality of retrieveing assembled frames from
     * playback queue respectively to their timestamps. Playout mechanism is 
     * self-correcting: it maintains measurements for each frame processing time
     * and adjusts for these drifts for the next frames.
     */
    class Playout : public IPlayout
    {
    public:
        Playout(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage> statStorage = 
                boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));

        void start(unsigned int fastForwardMs = 0) override;
        void stop() override;
        void addAdjustment(int64_t adjMs) override;

        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);
        void setDescription(const std::string& desc);
        bool isRunning() const;

        void attach(IPlayoutObserver* observer);
        void detach(IPlayoutObserver* observer);

    protected:
        Playout(boost::shared_ptr<PlayoutImpl> pimpl):pimpl_(pimpl){}

        PlayoutImpl* pimpl();
        PlayoutImpl* pimpl() const;

    private:
        boost::shared_ptr<PlayoutImpl> pimpl_;
    };
}

#endif /* defined(__ndnrtc__playout__) */
