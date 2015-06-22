//
//  interest-queue.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING

#include "interest-queue.h"
#include "consumer.h"

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
    startQueueWatching();
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
    queueAccess_.unlock();
    queueEvent_.notify_one();
    
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
InterestQueue::startQueueWatching()
{
    isWatchingQueue_ = true;
    queueWatchingThread_ = startThread([this]()->bool{
        return watchQueue();
    });
}

void
InterestQueue::stopQueueWatching()
{
    isWatchingQueue_ = false;
    queueEvent_.notify_one();
    stopThread(queueWatchingThread_);
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
    queueAccess_.unlock();
    
    if (entry.interest_.get())
        processEntry(entry);
    
    bool isQueueEmpty = false;
    queueAccess_.lock_shared();
    isQueueEmpty = (queue_.size() == 0);
    queueAccess_.unlock_shared();
    
    {
        unique_lock<mutex> lock(queueEventMutex_);
        if (isQueueEmpty)
            queueEvent_.wait(queueEventMutex_);
    }
    
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
