// 
// playout-impl.hpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playout_impl_h__
#define __playout_impl_h__

#include "ndnrtc-object.hpp"
#include "statistics.hpp"
#include "jitter-timing.hpp"

namespace ndnrtc {
	class IPlayoutObserver;
	class IPlaybackQueue;
    class BufferSlot;

    class PlayoutImpl : public NdnRtcComponent,
                        public statistics::StatObject
    {
    public:
        PlayoutImpl(boost::asio::io_service& io,
                const boost::shared_ptr<IPlaybackQueue>& queue,
                const boost::shared_ptr<statistics::StatisticsStorage> statStorage);
        ~PlayoutImpl();
        
        virtual void start(unsigned int fastForwardMs = 0);
        virtual void stop();
        
        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);
        void setDescription(const std::string& desc);
        bool isRunning() const { return isRunning_; }
        
        void attach(IPlayoutObserver* observer);
        void detach(IPlayoutObserver* observer);

        void addAdjustment(int64_t adjMs) { delayAdjustment_ += adjMs; }
    protected:
        PlayoutImpl(const PlayoutImpl&) = delete;
        
        mutable boost::recursive_mutex mutex_;
        std::atomic<bool> isRunning_;
        boost::shared_ptr<IPlaybackQueue> pqueue_;
        JitterTiming jitterTiming_;
        int64_t lastTimestamp_, lastDelay_, delayAdjustment_;
        std::vector<IPlayoutObserver*> observers_;
        
        void extractSample();
        virtual bool processSample(const boost::shared_ptr<const BufferSlot>&) { return false; }
        
        void correctAdjustment(int64_t newSampleTimestamp);
        int64_t adjustDelay(int64_t delay);
    };

    class IPlayoutObserver
    {
    public:
        virtual void onQueueEmpty() = 0;
        // virtual void onSamplePlayed() = 0;
    };
}

#endif
