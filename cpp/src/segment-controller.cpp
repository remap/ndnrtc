// 
// segment-controller.cpp
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "segment-controller.hpp"
#include "frame-data.hpp"
#include "async.hpp"
#include "clock.hpp"

#include <boost/thread/lock_guard.hpp>

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

namespace ndnrtc {
    class SegmentControllerImpl : public NdnRtcComponent,
                                  public ISegmentController,
                                  private Periodic
    {
    public:
        SegmentControllerImpl(boost::asio::io_service& faceIo, unsigned int
                              maxIdleTimeMs,
                              const boost::shared_ptr<StatisticsStorage>& storage);
        ~SegmentControllerImpl();
        
        unsigned int getCurrentIdleTime() const;
        unsigned int getMaxIdleTime() const { return maxIdleTimeMs_; }
        void setIsActive(bool active);
        bool getIsActive() const { return active_; }
        
        ndn::OnData getOnDataCallback();
        ndn::OnTimeout getOnTimeoutCallback();
        ndn::OnNetworkNack getOnNetworkNackCallback();
        
        void attach(ISegmentControllerObserver* o);
        void detach(ISegmentControllerObserver* o);
        
    private:
        bool active_;
        boost::mutex mutex_;
        unsigned int maxIdleTimeMs_;
        std::vector<ISegmentControllerObserver*> observers_;
        int64_t lastDataTimestampMs_;
        bool starvationFired_;
        boost::shared_ptr<StatisticsStorage> sstorage_;
        
        unsigned int periodicInvocation();
        
        void onData(const boost::shared_ptr<const ndn::Interest>&,
                    const boost::shared_ptr<ndn::Data>&);
        void onTimeout(const boost::shared_ptr<const ndn::Interest>&);
        void onNetworkNack(const boost::shared_ptr<const ndn::Interest>& interest,
                            const boost::shared_ptr<ndn::NetworkNack>& networkNack);
    };
}

#pragma mark - public
SegmentController::SegmentController(boost::asio::io_service& faceIo,
                                     unsigned int maxIdleTimeMs, StatStoragePtr storage):
pimpl_(boost::make_shared<SegmentControllerImpl>(faceIo, maxIdleTimeMs, storage))
{}

unsigned int SegmentController::getCurrentIdleTime() const
{
    return pimpl_->getCurrentIdleTime();
}

unsigned int SegmentController::getMaxIdleTime() const
{
    return pimpl_->getMaxIdleTime();
}

void SegmentController::setIsActive(bool active)
{
    pimpl_->setIsActive(active);
}

bool SegmentController::getIsActive() const
{
    return pimpl_->getIsActive();
}

ndn::OnData SegmentController::getOnDataCallback()
{
    return pimpl_->getOnDataCallback();
}

ndn::OnTimeout SegmentController::getOnTimeoutCallback()
{
    return pimpl_->getOnTimeoutCallback();
}

ndn::OnNetworkNack SegmentController::getOnNetworkNackCallback()
{
    return pimpl_->getOnNetworkNackCallback();    
}

void SegmentController::attach(ISegmentControllerObserver* o)
{
    pimpl_->attach(o);
}

void SegmentController::detach(ISegmentControllerObserver* o)
{
    pimpl_->detach(o);
}

void SegmentController::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    pimpl_->setLogger(logger);
}

#pragma mark - pimpl
SegmentControllerImpl::SegmentControllerImpl(boost::asio::io_service& faceIo,
	unsigned int maxIdleTimeMs,
    const boost::shared_ptr<StatisticsStorage>& storage):
Periodic(faceIo),
maxIdleTimeMs_(maxIdleTimeMs),
lastDataTimestampMs_(0),
starvationFired_(false),
active_(false),
sstorage_(storage)
{
	assert(sstorage_.get());
	description_ = "segment-controller";
}

SegmentControllerImpl::~SegmentControllerImpl(){
	cancelInvocation();
}

unsigned int SegmentControllerImpl::getCurrentIdleTime() const
{
	return 0;
}

void
SegmentControllerImpl::setIsActive(bool active)
{
    active_ = active;
    if (!active_)
        cancelInvocation();
    else
    {
        lastDataTimestampMs_ = clock::millisecondTimestamp();
        setupInvocation(maxIdleTimeMs_,
                        boost::bind(&SegmentControllerImpl::periodicInvocation, this));
    }
}

OnData SegmentControllerImpl::getOnDataCallback()
{
    boost::shared_ptr<SegmentControllerImpl> me = boost::dynamic_pointer_cast<SegmentControllerImpl>(shared_from_this());
	return boost::bind(&SegmentControllerImpl::onData, me, _1, _2);
}

OnTimeout SegmentControllerImpl::getOnTimeoutCallback()
{
    boost::shared_ptr<SegmentControllerImpl> me = boost::dynamic_pointer_cast<SegmentControllerImpl>(shared_from_this());
	return boost::bind(&SegmentControllerImpl::onTimeout, me, _1);
}

OnNetworkNack SegmentControllerImpl::getOnNetworkNackCallback()
{
    boost::shared_ptr<SegmentControllerImpl> me = boost::dynamic_pointer_cast<SegmentControllerImpl>(shared_from_this());
    return boost::bind(&SegmentControllerImpl::onNetworkNack, me, _1, _2);       
}

void SegmentControllerImpl::attach(ISegmentControllerObserver* o)
{
    if (o)
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.push_back(o);
    }
}

void SegmentControllerImpl::detach(ISegmentControllerObserver* o)
{
    std::vector<ISegmentControllerObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), o);
    if (it != observers_.end())
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}

#pragma mark - private
unsigned int SegmentControllerImpl::periodicInvocation()
{
	int64_t now = clock::millisecondTimestamp();

	if (now - lastDataTimestampMs_ >= maxIdleTimeMs_)
	{
		if (!starvationFired_)
		{
			LogWarnC << "no data during " << (now-lastDataTimestampMs_) << " ms" << std::endl;
	
			starvationFired_ = true;
			{
				boost::lock_guard<boost::mutex> scopedLock(mutex_);
				for (auto& o:observers_) o->segmentStarvation();
			}
		}
		return maxIdleTimeMs_;
	}

	return (maxIdleTimeMs_ - (now - lastDataTimestampMs_));
}

void SegmentControllerImpl::onData(const boost::shared_ptr<const Interest>& interest,
			const boost::shared_ptr<Data>& data)
{
    if (!active_)
    {
        LogWarnC << "incoming data in inactive state " << data->getName() << std::endl;
        return;
    }
    
	lastDataTimestampMs_ =  clock::millisecondTimestamp();
	starvationFired_ = false;
	NamespaceInfo info;

	if (NameComponents::extractInfo(data->getName(), info))
	{
		boost::shared_ptr<WireSegment> segment = WireSegment::createSegment(info, data, interest);

		if (segment->isValid())
		{
			LogTraceC << "received data " << data->getName() << " " 
				<< data->getContent().size() << " bytes" << std::endl;
			{
				boost::lock_guard<boost::mutex> scopedLock(mutex_);
				for (auto& o:observers_) o->segmentArrived(segment);
			}
            
			(*sstorage_)[Indicator::SegmentsReceivedNum]++;
			(*sstorage_)[Indicator::BytesReceived] += data->getContent().size();
			(*sstorage_)[Indicator::RawBytesReceived] += data->getDefaultWireEncoding().size();
		}
		else
			LogWarnC << "received invalid data " << data->getName() << std::endl;
	}
	else
		LogWarnC << "received data with bad name: " << data->getName() << std::endl;
}

void SegmentControllerImpl::onTimeout(const boost::shared_ptr<const Interest>& interest)
{
    if (!active_)
    {
        LogWarnC << "timeout in inactive state " << interest->getName() << std::endl;
        return;
    }
    
	NamespaceInfo info;

	if (NameComponents::extractInfo(interest->getName(), info))
	{
		LogTraceC << "received timeout for " << interest->getName() << std::endl;

		{
			boost::lock_guard<boost::mutex> scopedLock(mutex_);
			for (auto& o:observers_) o->segmentRequestTimeout(info);
		}

		(*sstorage_)[Indicator::TimeoutsNum]++;
	}
	else
		LogWarnC << "received timeout for badly named Interest " << interest->getName() << std::endl;
}

void SegmentControllerImpl::onNetworkNack(const boost::shared_ptr<const Interest>& interest,
                            const boost::shared_ptr<NetworkNack>& networkNack)
{
    if (!active_)
    {
        LogWarnC << "network nack (reason: " << networkNack->getReason() 
            << ", other: " << networkNack->getOtherReasonCode() 
            << ") in inactive state " << interest->getName() << std::endl;
        return;
    }

    NamespaceInfo info;

    if (NameComponents::extractInfo(interest->getName(), info))
    {
        LogTraceC << "received nack for " << interest->getName() << std::endl;
        
        {
            boost::lock_guard<boost::mutex> scopedLock(mutex_);
            int reason = (networkNack->getReason() == ndn_NetworkNackReason_OTHER_CODE ? networkNack->getOtherReasonCode() : networkNack->getReason());

            for (auto& o:observers_) o->segmentNack(info, reason);
        }

        (*sstorage_)[Indicator::NacksNum]++;
    }
    else
        LogWarnC << "received nack for badly named Interest " << interest->getName() << std::endl;
}
