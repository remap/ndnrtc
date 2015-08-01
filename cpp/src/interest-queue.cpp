//
//  interest-queue.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <boost/thread/lock_guard.hpp>

#include "interest-queue.h"
#include "consumer.h"
#include "ndnrtc-utils.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
InterestQueue::QueueEntry::QueueEntry(const Interest& interest,
                                      const shared_ptr<IPriority>& priority,
                                      const OnData& onData,
                                      const OnTimeout& onTimeout):
interest_(new Interest(interest)),
priority_(priority),
onDataCallback_(onData),
onTimeoutCallback_(onTimeout)
{
}

InterestQueue::InterestQueue(const shared_ptr<FaceWrapper>& face,
                             const boost::shared_ptr<statistics::StatisticsStorage>& statStorage):
StatObject(statStorage),
freqMeterId_(NdnRtcUtils::setupFrequencyMeter(10)),
face_(face),
queue_(PriorityQueue(IPriority::Comparator(true))),
isWatchingQueue_(false)
{
    description_ = "iqueue";
}

InterestQueue::~InterestQueue()
{
    stopQueueWatching();
}


//******************************************************************************
#pragma mark - public
void
InterestQueue::enqueueInterest(const Interest& interest,
                               const shared_ptr<IPriority>& priority,
                               const OnData& onData,
                               const OnTimeout& onTimeout)
{
    QueueEntry entry(interest, priority, onData, onTimeout);
    entry.setEnqueueTimestamp(NdnRtcUtils::millisecondTimestamp());
    
    queueAccess_.lock();
    queue_.push(entry);
    
    if (!isWatchingQueue_)
        scheduleJob(10, boost::bind(&InterestQueue::watchQueue, this));
    
    isWatchingQueue_ = true;
    queueAccess_.unlock();
    
    LogDebugC
    << "enqueue\t" << entry.interest_->getName()
    << "\texclude: " << entry.interest_->getExclude().toUri()
    << "\tpri: "
    << entry.getValue() << "\tlifetime: "
    << entry.interest_->getInterestLifetimeMilliseconds() << "\tqsize: "
    << queue_.size()
    << std::endl;
}

void
InterestQueue::reset()
{
    queueAccess_.lock();
    queue_ = PriorityQueue(IPriority::Comparator(true));
    NdnRtcUtils::releaseFrequencyMeter(freqMeterId_);
    freqMeterId_ = NdnRtcUtils::setupFrequencyMeter(10);
    queueAccess_.unlock();

    LogDebugC << "interest queue flushed" << std::endl;
}

//******************************************************************************
#pragma mark - private
void
InterestQueue::stopQueueWatching()
{
    stopJob();
    isWatchingQueue_ = false;
}

bool
InterestQueue::watchQueue()
{
    QueueEntry entry;
    
    queueAccess_.lock();
    if (queue_.size() > 0)
    {
        entry = queue_.top();
        queue_.pop();
    }
    isWatchingQueue_ = (queue_.size() > 0);
    queueAccess_.unlock();
    
    if (entry.interest_.get())
        processEntry(entry);
    
    return isWatchingQueue_;
}

void
InterestQueue::processEntry(const ndnrtc::new_api::InterestQueue::QueueEntry &entry)
{    
    LogDebugC
    << "express\t" << entry.interest_->getName()
    << "\texclude: " << entry.interest_->getExclude().toUri()
    << "\tpri: "
    << entry.getValue() << "\tlifetime: "
    << entry.interest_->getInterestLifetimeMilliseconds() << "\tqsize: "
    << queue_.size()
    << std::endl;
    
    NdnRtcUtils::frequencyMeterTick(freqMeterId_);
    (*statStorage_)[Indicator::InterestRate] = NdnRtcUtils::currentFrequencyMeterValue(freqMeterId_);
    (*statStorage_)[Indicator::QueueSize] = queue_.size();
    
    
    try {
        face_->expressInterest(*(entry.interest_),
                               entry.onDataCallback_, entry.onTimeoutCallback_);
        (*statStorage_)[Indicator::InterestsSentNum]++;
        
        if (callback_)
            callback_->onInterestIssued(entry.interest_);
    }
    catch (std::exception &e)
    {
        notifyError(RESULT_ERR, "got exception from NDN library: %s", e.what());
    }
}
