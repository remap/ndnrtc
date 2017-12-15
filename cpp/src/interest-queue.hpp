//
//  interest-queue.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__interest_queue__
#define __ndnrtc__interest_queue__

#include <queue>
#include <boost/asio.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>

#include "ndnrtc-object.hpp"
#include "statistics.hpp"

namespace ndn {
    class Interest;
    class Face;
    class Data;
    class NetworkNack;
}

namespace ndnrtc {
    class DeadlinePriority;

    typedef boost::function<void(const boost::shared_ptr<const ndn::Interest>&,
                                    const boost::shared_ptr<ndn::Data>&)> OnData;
    typedef boost::function<void(const boost::shared_ptr<const ndn::Interest>&)> 
            OnTimeout;
    typedef boost::function<void(const boost::shared_ptr<const ndn::Interest>& interest,
        const boost::shared_ptr<ndn::NetworkNack>& networkNack)> OnNetworkNack;

    class IInterestQueueObserver {
    public:
        virtual void onInterestIssued(const boost::shared_ptr<const ndn::Interest>&) = 0;
    };
    
    class IInterestQueue {
    public:
        virtual void
        enqueueInterest(const boost::shared_ptr<const ndn::Interest>& interest,
                        boost::shared_ptr<DeadlinePriority> priority,
                        OnData onData,
                        OnTimeout onTimeout,
                        OnNetworkNack onNetworkNack) = 0;
        virtual void reset() = 0;
    };

    /**
     * Interst queue class implements functionality for priority Interest queue.
     * Interests are expressed according to their priorities on Face thread.
     */
    class InterestQueue : public NdnRtcComponent,
                          public IInterestQueue,
                          public statistics::StatObject
    {
    public:

        class IPriority
        {
        public:
            virtual int64_t getValue() const = 0;
        };

        InterestQueue(boost::asio::io_service& io,
                      const boost::shared_ptr<ndn::Face> &face,
                      const boost::shared_ptr<statistics::StatisticsStorage>& statStorage);
        ~InterestQueue();
        
        /**
         * Enqueues Interest in the queue.
         * @param interest Interest to be expressed
         * @param priority Interest priority
         * @param onData OnData callback
         * @param onTimeout OnTimeout callback
         */
        void
        enqueueInterest(const boost::shared_ptr<const ndn::Interest>& interest,
                        boost::shared_ptr<DeadlinePriority> priority,
                        OnData onData,
                        OnTimeout onTimeout,
                        OnNetworkNack = OnNetworkNack());
        
        /**
         * Flushes current interest queue
         */
        void reset();
        void registerObserver(IInterestQueueObserver *observer) { observer_ = observer; }
        void unregisterObserver() { observer_ = nullptr; }
        size_t size() const { return queue_.size(); }
        
    private:
        class QueueEntry
        {
        public:
            class Comparator
            {
            public:
                Comparator(bool inverted):inverted_(inverted){}
                
                bool operator() (const QueueEntry& q1,
                                 const QueueEntry& q2) const
                {
                    return inverted_^(q1.getValue() < q2.getValue());
                }
                
            private:
                bool inverted_;
            };

            QueueEntry(const boost::shared_ptr<const ndn::Interest>& interest,
                       const boost::shared_ptr<IPriority>& priority,
                       OnData onData,
                       OnTimeout onTimeout,
                       OnNetworkNack onNetworkNack);

            int64_t
            getValue() const { return priority_->getValue(); }
            
            QueueEntry& operator=(const QueueEntry& entry)
            {
                interest_ = entry.interest_; // we just copy the pointer, since it's a pointer to a const
                priority_  = entry.priority_;
                onDataCallback_ = entry.onDataCallback_;
                onTimeoutCallback_ = entry.onTimeoutCallback_;
                onNetworkNack_ = entry.onNetworkNack_;
                return *this;
            }

        private:
            friend InterestQueue;

            boost::shared_ptr<const ndn::Interest> interest_;
            boost::shared_ptr<IPriority> priority_;
            OnData onDataCallback_;
            OnTimeout onTimeoutCallback_;
            OnNetworkNack onNetworkNack_;
        };
        
        typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>, 
                    QueueEntry::Comparator> PriorityQueue;
        
        boost::shared_ptr<ndn::Face> face_;
        boost::asio::io_service& faceIo_;
        boost::recursive_mutex queueAccess_;
        PriorityQueue queue_;
        IInterestQueueObserver *observer_;
        bool isDrainingQueue_;
        
        void safeDrain();
        void drainQueue();
        void stopQueueWatching();
        void processEntry(const QueueEntry &entry);
    };
    
    /**
     * DeadlinePriority class implements IPriority interface for assigning 
     * priorities for Interests based on expected data arrival deadlines.
     * The farther the arrival deadline is, the lower the priority.
     */
    class DeadlinePriority : public InterestQueue::IPriority
    {
    public:
        DeadlinePriority(int64_t arrivalDelay);

        int64_t getValue() const;
        void setEnqueueTimestamp(int64_t timestamp) { enqueuedMs_ = timestamp; }

        static boost::shared_ptr<DeadlinePriority>
        fromNow(int64_t delayMs) { return boost::make_shared<DeadlinePriority>(delayMs); }
        
    private:
        int64_t enqueuedMs_, arrivalDelayMs_;

        DeadlinePriority(const DeadlinePriority& p);
        int64_t getArrivalDeadlineFromEnqueue() const;
    };
}

#endif /* defined(__ndnrtc__interest_queue__) */
