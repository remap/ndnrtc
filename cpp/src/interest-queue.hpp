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

#define REQ_DL_PRI_DEFAULT  150
#define REQ_DL_PRI_RTX      50
#define REQ_DL_PRI_URGENT   0

namespace ndn {
    class Interest;
    class Face;
    class Data;
    class NetworkNack;
}

namespace ndnlog {
namespace new_api {

    class Logger;

}
}

namespace ndnrtc {
    class DeadlinePriority;
    class DataRequest;
    class RequestQueueImpl;

    typedef std::function<void(const std::shared_ptr<const ndn::Interest>&,
                                    const std::shared_ptr<ndn::Data>&)> OnData;
    typedef std::function<void(const std::shared_ptr<const ndn::Interest>&)>
            OnTimeout;
    typedef std::function<void(const std::shared_ptr<const ndn::Interest>& interest,
        const std::shared_ptr<ndn::NetworkNack>& networkNack)> OnNetworkNack;
    typedef std::function<void(const std::shared_ptr<const ndn::Interest>&)>
            OnExpressInterest;
    typedef std::function<void()> DelayedCallback;

    class IInterestQueueObserver {
    public:
        virtual void onInterestIssued(const std::shared_ptr<const ndn::Interest>&) = 0;
    };

    class IInterestQueue {
    public:
        virtual void
        enqueueInterest(const std::shared_ptr<const ndn::Interest>& interest,
                        std::shared_ptr<DeadlinePriority> priority,
                        OnData onData,
                        OnTimeout onTimeout,
                        OnNetworkNack onNetworkNack) = 0;

        virtual void
        enqueueRequest(std::shared_ptr<DataRequest>& request,
                       std::shared_ptr<DeadlinePriority> priority) = 0;

        virtual void
        enqueueRequests(std::vector<std::shared_ptr<DataRequest>>& requests,
                        std::shared_ptr<DeadlinePriority> priority) = 0;

        virtual void reset() = 0;
    };

    /**
     * Interst queue class implements functionality for priority Interest queue.
     * Interests are expressed according to their priorities on Face thread.
     */
    class RequestQueue : public IInterestQueue
    {
    public:
        class IPriority
        {
        public:
            virtual int64_t getValue() const = 0;
        };

        RequestQueue(boost::asio::io_service& io,
                     const std::shared_ptr<ndn::Face> &face,
                     const std::shared_ptr<statistics::StatisticsStorage>& statStorage);
        RequestQueue(boost::asio::io_service& io,
                     ndn::Face *face,
                     const std::shared_ptr<statistics::StatisticsStorage>& statStorage);
        RequestQueue(boost::asio::io_service& io,
                     ndn::Face *face);
//        InterestQueue(const ndn::Face &face,
//                      const std::shared_ptr<statistics::StatisticsStorage>& statStorage){}
        ~RequestQueue();

        /**
         * Enqueues Interest in the queue.
         * @param interest Interest to be expressed
         * @param priority Interest priority
         * @param onData OnData callback
         * @param onTimeout OnTimeout callback
         */
        void
        enqueueInterest(const std::shared_ptr<const ndn::Interest>& interest,
                        std::shared_ptr<DeadlinePriority> priority,
                        OnData onData,
                        OnTimeout onTimeout,
                        OnNetworkNack = OnNetworkNack()) DEPRECATED;
        /**
         * Enqueues DataRequest in the queue.
         * All callbacks will be dispatched through the DataRequest object's status update events.
         * @param request DataRequest to be processed
         * @param priority DataRequest priority
         */
        void
        enqueueRequest(std::shared_ptr<DataRequest>& request,
                       std::shared_ptr<DeadlinePriority> priority =
                            std::make_shared<DeadlinePriority>(REQ_DL_PRI_DEFAULT));

        void
        enqueueRequests(std::vector<std::shared_ptr<DataRequest>>& requests,
                        std::shared_ptr<DeadlinePriority> priority =
                            std::make_shared<DeadlinePriority>(REQ_DL_PRI_DEFAULT));
        /**
         * Only call that if interest queue was not initialized using io_service
         */
//        void
//        process();

        /**
         * Flushes current interest queue
         */
        void reset();


        size_t size() const;
        void setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger);
        double getDrdEstimate() const;
        double getJitterEstimate() const;
        const statistics::StatisticsStorage& getStatistics() const;
        void callLater(uint64_t usecDelay, DelayedCallback callback);
        boost::asio::io_service& getIoService() const;

        void registerObserver(IInterestQueueObserver *observer) DEPRECATED;
        void unregisterObserver() DEPRECATED;
    private:
        std::shared_ptr<RequestQueueImpl> pimpl_;
    };

    typedef RequestQueue InterestQueue;

    /**
     * DeadlinePriority class implements IPriority interface for assigning
     * priorities for Interests based on expected data arrival deadlines.
     * The farther the arrival deadline is, the lower the priority.
     */
    class DeadlinePriority : public RequestQueue::IPriority
    {
    public:
        DeadlinePriority(int64_t arrivalDelay);

        int64_t getValue() const;
        void setEnqueueTimestamp(int64_t timestamp) { enqueuedMs_ = timestamp; }

        static std::shared_ptr<DeadlinePriority>
        fromNow(int64_t delayMs) { return std::make_shared<DeadlinePriority>(delayMs); }

    private:
        int64_t enqueuedMs_, arrivalDelayMs_;

        DeadlinePriority(const DeadlinePriority& p);
        int64_t getArrivalDeadlineFromEnqueue() const;
    };
}

#endif /* defined(__ndnrtc__interest_queue__) */
