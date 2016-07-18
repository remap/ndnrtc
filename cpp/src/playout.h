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

#ifndef __ndnrtc__playout__
#define __ndnrtc__playout__

#include <boost/asio.hpp>

#include "statistics.h"
#include "ndnrtc-object.h"
#include "jitter-timing.h"

namespace ndnlog {
    namespace new_api {
        class Logger;
    }
}

namespace ndnrtc {
    class BufferSlot;
    class IPlaybackQueue;
    class IPlayoutObserver;
    typedef statistics::StatisticsStorage StatStorage;

    class IPlayout {
    public:
        virtual void start(unsigned int fastForwardMs = 0) = 0;
        virtual void stop() = 0;
        virtual bool isRunning() const = 0;
    };

    /**
     * Playout is a base class for media streams (audio and video) playout 
     * mechanism. It implements functionality of retrieveing assembled frames from
     * playback queue respectively to their timestamps. Playout mechanism is 
     * self-correcting: it maintains measurements for each frame processing time
     * and adjusts for these drifts for the next frames.
     */
    class Playout : public NdnRtcComponent, 
                    public IPlayout,
                    public statistics::StatObject
    {
    public:
        Playout(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage> statStorage = 
                boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));
        ~Playout();

        virtual void start(unsigned int fastForwardMs = 0);
        virtual void stop();

        void setLogger(ndnlog::new_api::Logger* logger);
        void setDescription(const std::string& desc);
        bool isRunning() const { return isRunning_; }

        void attach(IPlayoutObserver* observer);
        void detach(IPlayoutObserver* observer);

    protected:
        Playout(const Playout&) = delete;

        mutable boost::recursive_mutex mutex_;
        boost::atomic<bool> isRunning_;
        boost::shared_ptr<IPlaybackQueue> pqueue_;
        JitterTiming jitterTiming_;
        int64_t lastTimestamp_, lastDelay_, delayAdjustment_;
        std::vector<IPlayoutObserver*> observers_;

        void extractSample();
        virtual void processSample(const boost::shared_ptr<const BufferSlot>&) = 0;
        
        void correctAdjustment(int64_t newSampleTimestamp);
        int64_t adjustDelay(int64_t delay);
    };

    class IPlayoutObserver
    {
    public:
        virtual void onQueueEmpty() = 0;
    };
}

#endif /* defined(__ndnrtc__playout__) */
