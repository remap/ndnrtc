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
#include "estimators.hpp"

// time window for estimating network parameters
#define NW_ESTIMATE_WINDOW 30000

using namespace std;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

namespace ndnrtc
{

class RequestQueueImpl : public NdnRtcComponent,
                         public statistics::StatObject
{
public:
    RequestQueueImpl(boost::asio::io_service& io,
                     ndn::Face *face,
                     const boost::shared_ptr<statistics::StatisticsStorage>& statStorage);
    ~RequestQueueImpl();
    
    void
    enqueueInterest(const boost::shared_ptr<const Interest>& interest,
                    boost::shared_ptr<DeadlinePriority> priority,
                    OnData onData,
                    OnTimeout onTimeout,
                    OnNetworkNack onNetworkNack);
    
    void
    enqueueRequest(boost::shared_ptr<DataRequest>& request,
                   boost::shared_ptr<DeadlinePriority> priority);
    
    void reset();
    void registerObserver(IInterestQueueObserver *observer) { observer_ = observer; }
    void unregisterObserver() { observer_ = nullptr; }
    size_t size() const { return queue_.size(); }
    const estimators::Filter& getEstimator() const { return drdEstimator_; }
    
    boost::asio::io_service& getFaceIo() const { return faceIo_; }
    void callLater(uint64_t usecDelay, DelayedCallback callback);
    
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
                   const boost::shared_ptr<RequestQueue::IPriority>& priority,
                   OnData onData,
                   OnTimeout onTimeout,
                   OnNetworkNack onNetworkNack,
                   OnExpressInterest onExpressInterest = OnExpressInterest());
        QueueEntry(QueueEntry&& qe) = default;
        
        int64_t
        getValue() const { return priority_->getValue(); }
        
        QueueEntry& operator=(const QueueEntry& entry)
        {
            interest_ = entry.interest_; // we just copy the pointer, since it's a pointer to a const
            priority_  = entry.priority_;
            onDataCallback_ = entry.onDataCallback_;
            onTimeoutCallback_ = entry.onTimeoutCallback_;
            onNetworkNack_ = entry.onNetworkNack_;
            onExpressInterest_ = entry.onExpressInterest_;
            return *this;
        }
        
    private:
        friend RequestQueueImpl;
        
        boost::shared_ptr<const ndn::Interest> interest_;
        boost::shared_ptr<RequestQueue::IPriority> priority_;
        OnData onDataCallback_;
        OnTimeout onTimeoutCallback_;
        OnNetworkNack onNetworkNack_;
        OnExpressInterest onExpressInterest_;
    };
    
    typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>,
    QueueEntry::Comparator> PriorityQueue;
    
    ndn::Face *face_;
    boost::asio::io_service& faceIo_;
    boost::recursive_mutex queueAccess_;
    PriorityQueue queue_;
    IInterestQueueObserver *observer_;
    bool isDrainingQueue_;
    estimators::Filter drdEstimator_;
    uint64_t lastRequestId_, lastRcvdRequestId_;
    
    void enqueue(QueueEntry&, boost::shared_ptr<DeadlinePriority> priority);
    void drainQueueSafe();
    void drainQueue();
    void processEntry(const QueueEntry &entry);
};

}

//******************************************************************************
RequestQueue::RequestQueue(boost::asio::io_service& io,
                           const boost::shared_ptr<ndn::Face> &face,
                           const boost::shared_ptr<StatisticsStorage>& statStorage)
: pimpl_(boost::make_shared<RequestQueueImpl>(io, face.get(), statStorage))
{}

RequestQueue::RequestQueue(boost::asio::io_service& io,
                           ndn::Face *face,
                           const boost::shared_ptr<StatisticsStorage>& statStorage)
: pimpl_(boost::make_shared<RequestQueueImpl>(io, face, statStorage))
{}

RequestQueue::RequestQueue(boost::asio::io_service& io,
                           ndn::Face *face)
: pimpl_(boost::make_shared<RequestQueueImpl>(io, face,
                                              boost::shared_ptr<StatisticsStorage>(StatisticsStorage::createConsumerStatistics())))
{
}

RequestQueue::~RequestQueue()
{
}

void
RequestQueue::enqueueInterest(const boost::shared_ptr<const ndn::Interest> &interest,
                              boost::shared_ptr<DeadlinePriority> priority,
                              OnData onData,
                              OnTimeout onTimeout,
                              OnNetworkNack onNetworkNack)
{
    pimpl_->enqueueInterest(interest, priority, onData, onTimeout, onNetworkNack);
}

void
RequestQueue::enqueueRequest(boost::shared_ptr<DataRequest> &request,
                             boost::shared_ptr<DeadlinePriority> priority)
{
    pimpl_->enqueueRequest(request, priority);
}

void
RequestQueue::enqueueRequests(vector<boost::shared_ptr<DataRequest>>& requests,
                boost::shared_ptr<DeadlinePriority> priority)
{
    for (auto &r:requests)
        pimpl_->enqueueRequest(r, priority);
}

void RequestQueue::reset() { pimpl_->reset(); }
void RequestQueue::registerObserver(IInterestQueueObserver *observer) { pimpl_->registerObserver(observer); }
void RequestQueue::unregisterObserver() { pimpl_->unregisterObserver(); }
size_t RequestQueue::size() const { return pimpl_->size(); }
void RequestQueue::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger) { pimpl_->setLogger(logger); }
double RequestQueue::getDrdEstimate() const { return pimpl_->getEstimator().value(); }
double RequestQueue::getJitterEstimate() const { return pimpl_->getEstimator().variation(); }
const statistics::StatisticsStorage& RequestQueue::getStatistics() const { return pimpl_->getStatistics(); }
void RequestQueue::callLater(uint64_t usecDelay, DelayedCallback callback) { pimpl_->callLater(usecDelay, callback); }
boost::asio::io_service& RequestQueue::getIoService() const { return pimpl_->getFaceIo(); }

//******************************************************************************
#pragma mark - construction/destruction
RequestQueueImpl::QueueEntry::QueueEntry(const boost::shared_ptr<const ndn::Interest>& interest,
                                         const boost::shared_ptr<RequestQueue::IPriority>& priority,
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
RequestQueueImpl::RequestQueueImpl(boost::asio::io_service& io,
                      Face *face,
                      const boost::shared_ptr<statistics::StatisticsStorage>& statStorage)
: StatObject(statStorage)
, faceIo_(io)
, face_(face)
, queue_(PriorityQueue(QueueEntry::Comparator(true)))
, isDrainingQueue_(false)
, observer_(nullptr)
//, drdEstimator_(boost::make_shared<estimators::TimeWindow>(NW_ESTIMATE_WINDOW))
, drdEstimator_(15./16., 15./16.)
, lastRequestId_(0)
, lastRcvdRequestId_(0)
{
    description_ = "iqueue";
}

RequestQueueImpl::~RequestQueueImpl()
{
    reset();
}


//******************************************************************************
#pragma mark - public
void
RequestQueueImpl::enqueueInterest(const boost::shared_ptr<const Interest>& interest,
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
RequestQueueImpl::enqueueRequest(boost::shared_ptr<DataRequest> &request,
                                 boost::shared_ptr<DeadlinePriority> priority)
{
    assert(request.get());
    
    boost::shared_ptr<InterestQueue> me = boost::dynamic_pointer_cast<InterestQueue>(shared_from_this());
    QueueEntry entry(request->getInterest(), priority,
                     [request, me, this](const boost::shared_ptr<const Interest>&,
                               const boost::shared_ptr<Data>& d){
                         request->timestampReply();
                         request->setData(d);
                         
                         if (request->getId() < lastRcvdRequestId_)
                             (*statStorage_)[Indicator::OutOfOrderNum]++;
                         else
                             lastRcvdRequestId_ = request->getId();
                         
                         drdEstimator_.newValue(request->getDrdUsec());
                         
                         if (d->getMetaInfo().getType() == ndn_ContentType_NACK)
                         {
                             LogTraceC << "app nack " << d->getName() << endl;
                             
                             request->setStatus(DataRequest::Status::AppNack);
                             request->triggerEvent(DataRequest::Status::AppNack);
                         }
                         else
                         {
                             LogTraceC << "data " << d->getName() << endl;
                             
                             request->setStatus(DataRequest::Status::Data);
                             request->triggerEvent(DataRequest::Status::Data);
                         }
                     },
                     [request, me, this](const boost::shared_ptr<const Interest>& i){
                         LogTraceC << "timeout " << i->getName() << endl;
                         
                         request->timestampReply();
                         request->setTimeout();
                         request->setStatus(DataRequest::Status::Timeout);
                         request->triggerEvent(DataRequest::Status::Timeout);
                     },
                     [request, me, this](const boost::shared_ptr<const ndn::Interest>& interest,
                               const boost::shared_ptr<ndn::NetworkNack>& networkNack){
                         LogTraceC << "network nack " << interest->getName() << " - " << networkNack->getReason() << endl;
                         
                         request->timestampReply();
                         request->setNack(networkNack);
                         request->setStatus(DataRequest::Status::NetworkNack);
                         request->triggerEvent(DataRequest::Status::NetworkNack);
                     },
                     [request, me, this](const boost::shared_ptr<const Interest>& i){
                         lastRequestId_++;
                         request->setId(lastRequestId_);
                         if (request->getStatus() != DataRequest::Status::Created)
                             request->setRtx();
                         request->timestampRequest();
                         assert(request->getInterest()->getName() == i->getName());
                         request->setStatus(DataRequest::Status::Expressed);
                         request->triggerEvent(DataRequest::Status::Expressed);
                     });
    enqueue(entry, priority);
}

void
RequestQueueImpl::reset()
{
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
        queue_ = PriorityQueue(QueueEntry::Comparator(true));
    }

    LogDebugC << "queue flushed" << std::endl;
}

void
RequestQueueImpl::callLater(uint64_t usecDelay, DelayedCallback callback)
{
    face_->callLater(usecDelay/1000, callback);
}

//******************************************************************************
#pragma mark - private
void
RequestQueueImpl::enqueue(QueueEntry &qe, boost::shared_ptr<DeadlinePriority> priority)
{
    priority->setEnqueueTimestamp(clock::millisecondTimestamp());
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
        queue_.emplace(move(qe));
        
        if (!isDrainingQueue_)
        {
            isDrainingQueue_ = true;
            boost::shared_ptr<RequestQueueImpl> me =
                boost::dynamic_pointer_cast<RequestQueueImpl>(shared_from_this());
            async::dispatchAsync(faceIo_, boost::bind(&RequestQueueImpl::drainQueueSafe, me));
        }
    }
}

void
RequestQueueImpl::drainQueueSafe()
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(queueAccess_);
    drainQueue();    
}

void 
RequestQueueImpl::drainQueue()
{
    isDrainingQueue_ = (queue_.size() > 0);
    
    while (isDrainingQueue_)
    {
        processEntry(queue_.top());
        queue_.pop();
        isDrainingQueue_ = (queue_.size() > 0);
    }
}

void
RequestQueueImpl::processEntry(const RequestQueueImpl::QueueEntry &entry)
{    
    LogTraceC << " pri " << entry.getValue()
              << " " << entry.interest_->getInterestLifetimeMilliseconds()
              << "ms qsz " << queue_.size()
              << " mbf " << entry.interest_->getMustBeFresh()
              << " " << entry.interest_->getName()
              << std::endl;
    
    face_->expressInterest(*(entry.interest_), entry.onDataCallback_,
        entry.onTimeoutCallback_, entry.onNetworkNack_);

    if (entry.onExpressInterest_)
        entry.onExpressInterest_(entry.interest_);
    
    (*statStorage_)[Indicator::QueueSize] = queue_.size();
    (*statStorage_)[Indicator::InterestsSentNum]++;

    if (observer_) observer_->onInterestIssued(entry.interest_);
}
