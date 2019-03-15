//
//  interest-queue.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "interest-queue.hpp"
#include <boost/thread/lock_guard.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/interest.hpp>

#include "clock.hpp"
#include "async.hpp"
#include "network-data.hpp"

using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

//******************************************************************************
#pragma mark - construction/destruction
InterestQueue::QueueEntry::QueueEntry(const boost::shared_ptr<const ndn::Interest>& interest,
                                      const boost::shared_ptr<IPriority>& priority,
                                      OnData onData,
                                      OnTimeout onTimeout,
                                      OnNetworkNack onNetworkNack,
                                      OnExpressInterest onExpressInterest):
interest_(interest),
priority_(priority),
onDataCallback_(onData),
onTimeoutCallback_(onTimeout),
onNetworkNack_(onNetworkNack),
onExpressInterest_(onExpressInterest)
{
}

//******************************************************************************
DeadlinePriority::DeadlinePriority(const DeadlinePriority& p):
arrivalDelayMs_(p.arrivalDelayMs_),
enqueuedMs_(0)
{}

DeadlinePriority::DeadlinePriority(int64_t arrivalDelay):
arrivalDelayMs_(arrivalDelay),
enqueuedMs_(0)
{}

int64_t
DeadlinePriority::getValue() const
{
    return getArrivalDeadlineFromEnqueue() - clock::millisecondTimestamp();
}

int64_t
DeadlinePriority::getArrivalDeadlineFromEnqueue() const
{
    if (enqueuedMs_ <= 0) return -1;
    return enqueuedMs_+arrivalDelayMs_;
}

//******************************************************************************
InterestQueue::InterestQueue(boost::asio::io_service& io,
                      const boost::shared_ptr<Face> &face,
                      const boost::shared_ptr<statistics::StatisticsStorage>& statStorage):
StatObject(statStorage),
faceIo_(io),
face_(face),
queue_(PriorityQueue(QueueEntry::Comparator(true))),
isDrainingQueue_(false),
observer_(nullptr)
{
    description_ = "iqueue";
}

InterestQueue::~InterestQueue()
{
    reset();
}


//******************************************************************************
#pragma mark - public
void
InterestQueue::enqueueInterest(const boost::shared_ptr<const Interest>& interest,
                               boost::shared_ptr<DeadlinePriority> priority,
                               OnData onData,
                               OnTimeout onTimeout,
                               OnNetworkNack onNetworkNack)
{
    assert(interest.get());

    QueueEntry entry(interest, priority, onData, onTimeout, onNetworkNack);
    enqueue(entry, priority);
}

void
InterestQueue::enqueueRequest(boost::shared_ptr<DataRequest> &request,
                              boost::shared_ptr<DeadlinePriority> priority)
{
    assert(request.get());
    
    QueueEntry entry(request->getInterest(), priority,
                     [request](const boost::shared_ptr<const Interest>&,
                               const boost::shared_ptr<Data>& d){
                         request->timestampReply();
                         request->setData(d);
                         
                         if (d->getMetaInfo().getType() == ndn_ContentType_NACK)
                         {
                             request->setStatus(DataRequest::Status::AppNack);
                             request->triggerEvent(DataRequest::Status::AppNack);
                         }
                         else
                         {
                             request->setStatus(DataRequest::Status::Data);
                             request->triggerEvent(DataRequest::Status::Data);
                         }
                     },
                     [request](const boost::shared_ptr<const Interest>&){
                         request->timestampReply();
                         request->setTimeout();
                         request->setStatus(DataRequest::Status::Timeout);
                         request->triggerEvent(DataRequest::Status::Timeout);
                     },
                     [request](const boost::shared_ptr<const ndn::Interest>& interest,
                               const boost::shared_ptr<ndn::NetworkNack>& networkNack){
                         request->timestampReply();
                         request->setNack(networkNack);
                         request->setStatus(DataRequest::Status::NetworkNack);
                         request->triggerEvent(DataRequest::Status::NetworkNack);
                     },
                     [request](const boost::shared_ptr<const Interest>&){
                         if (request->getStatus() != DataRequest::Status::Created)
                             request->setRtx();
                         request->timestampRequest();
                         request->setStatus(DataRequest::Status::Expressed);
                         request->triggerEvent(DataRequest::Status::Expressed);
                     });
    enqueue(entry, priority);
}

void
InterestQueue::reset()
{
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
        queue_ = PriorityQueue(QueueEntry::Comparator(true));
    }

    LogDebugC << "queue flushed" << std::endl;
}

//******************************************************************************
#pragma mark - private
void
InterestQueue::enqueue(QueueEntry &qe, boost::shared_ptr<DeadlinePriority> priority)
{
    priority->setEnqueueTimestamp(clock::millisecondTimestamp());
    
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
        queue_.push(qe);
        
        if (!isDrainingQueue_)
        {
            isDrainingQueue_ = true;
            async::dispatchAsync(faceIo_, boost::bind(&InterestQueue::safeDrain, this));
        }
        else
            if (queue_.size() > 10)
                // async::dispatchSync(faceIo_, boost::bind(&InterestQueue::drainQueue, this));
                drainQueue();   // this is a hack and it will break everything if enqueueInterest
        // is called from other than faceIo_ thread. however, the code
        // above locks and I don't know how to avoid growing queues in
        // io_service other than draining them forcibly
    }
    
    // LogTraceC
    // << "enqueue\t" << entry.interest_->getName()
    // << "\texclude: " << entry.interest_->getExclude().toUri()
    // << "\tpri: "
    // << entry.getValue() << "\tlifetime: "
    // << entry.interest_->getInterestLifetimeMilliseconds()
    // << "\tqsize: " << queue_.size() << std::endl;
}

void
InterestQueue::safeDrain()
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
    drainQueue();    
}

void 
InterestQueue::drainQueue()
{
    // boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
    isDrainingQueue_ = (queue_.size() > 0);

    if (isDrainingQueue_)
    {
        // LogTraceC 
        // << "draining queue, size "  << queue_.size() 
        // << ", top priority: " << queue_.top().getValue() << std::endl;

        while (isDrainingQueue_)
        {
            processEntry(queue_.top());
            queue_.pop();
            isDrainingQueue_ = (queue_.size() > 0);
        }
    }
}

void
InterestQueue::processEntry(const InterestQueue::QueueEntry &entry)
{    
    LogTraceC << "express\t" << entry.interest_->getName()
              << "\texclude: " << entry.interest_->getExclude().toUri()
              << "\tpri: " << entry.getValue() 
              << "\tlifetime: " << entry.interest_->getInterestLifetimeMilliseconds()
              << "\tqsize: " << queue_.size()
              << "\tmustBeFresh: " << entry.interest_->getMustBeFresh()
              << std::endl;

    face_->expressInterest(*(entry.interest_), entry.onDataCallback_, 
        entry.onTimeoutCallback_, entry.onNetworkNack_);
    
    if (entry.onExpressInterest_)
        entry.onExpressInterest_(entry.interest_);
    
    (*statStorage_)[Indicator::QueueSize] = queue_.size();
    (*statStorage_)[Indicator::InterestsSentNum]++;

    if (observer_) observer_->onInterestIssued(entry.interest_);
}
