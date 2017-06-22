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

using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

//******************************************************************************
#pragma mark - construction/destruction
InterestQueue::QueueEntry::QueueEntry(const boost::shared_ptr<const ndn::Interest>& interest,
                                      const boost::shared_ptr<IPriority>& priority,
                                      OnData onData,
                                      OnTimeout onTimeout,
                                      OnNetworkNack onNetworkNack):
interest_(interest),
priority_(priority),
onDataCallback_(onData),
onTimeoutCallback_(onTimeout),
onNetworkNack_(onNetworkNack)
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
    priority->setEnqueueTimestamp(clock::millisecondTimestamp());
    
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
        queue_.push(entry);
    
        if (!isDrainingQueue_)
        {
            isDrainingQueue_ = true;
            async::dispatchAsync(faceIo_,
                boost::bind(&InterestQueue::drainQueue, this));
        }
    }
    
    LogTraceC
    << "enqueue\t" << entry.interest_->getName()
    << "\texclude: " << entry.interest_->getExclude().toUri()
    << "\tpri: "
    << entry.getValue() << "\tlifetime: "
    << entry.interest_->getInterestLifetimeMilliseconds()
    << "\tqsize: " << queue_.size() << std::endl;
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
InterestQueue::drainQueue()
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
    while (isDrainingQueue_)
    {
        processEntry(queue_.top());
        queue_.pop();
        isDrainingQueue_ = (queue_.size() > 0);
    }
}

void
InterestQueue::processEntry(const InterestQueue::QueueEntry &entry)
{    
    LogTraceC
    << " express\t" << entry.interest_->getName()
    << "\texclude: " << entry.interest_->getExclude().toUri()
    << "\tpri: "
    << entry.getValue() << "\tlifetime: "
    << entry.interest_->getInterestLifetimeMilliseconds() << "\tqsize: "
    << queue_.size()
    << std::endl;

    face_->expressInterest(*(entry.interest_), entry.onDataCallback_, 
        entry.onTimeoutCallback_, entry.onNetworkNack_);
    
    (*statStorage_)[Indicator::QueueSize] = queue_.size();
    (*statStorage_)[Indicator::InterestsSentNum]++;

    if (observer_) observer_->onInterestIssued(entry.interest_);
}
