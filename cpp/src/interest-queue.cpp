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

InterestQueue::InterestQueue(const shared_ptr<FaceWrapper>& face):
freqMeterId_(NdnRtcUtils::setupFrequencyMeter(10)),
face_(face),
queueAccess_(*RWLockWrapper::CreateRWLock()),
queueEvent_(*EventWrapper::Create()),
queueWatchingThread_(*ThreadWrapper::CreateThread(InterestQueue::watchThreadRoutine, this)),
queue_(PriorityQueue(IPriority::Comparator(true))),
isWatchingQueue_(false)
{
    description_ = "iqueue";
    startQueueWatching();
}

InterestQueue::~InterestQueue()
{
    stopQueueWatching();
    
    queueWatchingThread_.~ThreadWrapper();
    queueAccess_.~RWLockWrapper();
    queueEvent_.~EventWrapper();
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
    
    queueAccess_.AcquireLockExclusive();
    queue_.push(entry);
    queueAccess_.ReleaseLockExclusive();
    
    queueEvent_.Set();
    
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
    queueAccess_.AcquireLockExclusive();
    queue_ = PriorityQueue(IPriority::Comparator(true));
    queueAccess_.ReleaseLockExclusive();

    LogDebugC << "interest queue flushed" << std::endl;
}

void
InterestQueue::getStatistics(ReceiverChannelPerformance& stat)
{
    stat.interestFrequency_ = NdnRtcUtils::currentFrequencyMeterValue(freqMeterId_);
}

//******************************************************************************
#pragma mark - private
void
InterestQueue::startQueueWatching()
{
    isWatchingQueue_ = true;
    
    unsigned int tid;
    queueWatchingThread_.Start(tid);
}

void
InterestQueue::stopQueueWatching()
{
    queueWatchingThread_.SetNotAlive();
    isWatchingQueue_ = false;
    queueEvent_.Set();
    queueWatchingThread_.Stop();
}

bool
InterestQueue::watchQueue()
{
    QueueEntry entry;
    
    queueAccess_.AcquireLockExclusive();
    if (queue_.size() > 0)
    {
        entry = queue_.top();
        queue_.pop();
    }
    queueAccess_.ReleaseLockExclusive();
    
    if (entry.interest_.get())
        processEntry(entry);
    
    bool isQueueEmpty = false;
    queueAccess_.AcquireLockShared();
    isQueueEmpty = (queue_.size() == 0);
    queueAccess_.ReleaseLockShared();
    
    if (isQueueEmpty)
        queueEvent_.Wait(WEBRTC_EVENT_INFINITE);
    
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
    
    try {
        face_->expressInterest(*(entry.interest_),
                               entry.onDataCallback_, entry.onTimeoutCallback_);
        if (callback_)
            callback_->onInterestIssued(entry.interest_);
    }
    catch (std::exception &e)
    {
        notifyError(RESULT_ERR, "got exception from NDN library: %s", e.what());
    }
}
