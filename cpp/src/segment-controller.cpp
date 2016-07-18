// 
// segment-controller.cpp
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "segment-controller.h"
#include "frame-data.h"
#include "async.h"
#include "clock.h"

#include <boost/thread/lock_guard.hpp>

using namespace ndnrtc;
using namespace ndn;

#pragma mark - public
SegmentController::SegmentController(boost::asio::io_service& faceIo,
	unsigned int maxIdleTimeMs):
Periodic(faceIo),
maxIdleTimeMs_(maxIdleTimeMs),
lastDataTimestampMs_(clock::millisecondTimestamp()),
starvationFired_(false)
{
	description_ = "segment-controller";
	setupInvocation(maxIdleTimeMs_, 
		boost::bind(&SegmentController::periodicInvocation, this));
}

SegmentController::~SegmentController(){
	cancelInvocation();
}

unsigned int SegmentController::getCurrentIdleTime() const
{
	return 0;
}

OnData SegmentController::getOnDataCallback()
{
	return boost::bind(&SegmentController::onData, this, _1, _2);
}

OnTimeout SegmentController::getOnTimeoutCallback()
{
	return boost::bind(&SegmentController::onTimeout, this, _1);
}

void SegmentController::attach(ISegmentControllerObserver* o)
{
    if (o)
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.push_back(o);
    }
}

void SegmentController::detach(ISegmentControllerObserver* o)
{
    std::vector<ISegmentControllerObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), o);
    if (it != observers_.end())
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}

#pragma mark - private
unsigned int SegmentController::periodicInvocation()
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

void SegmentController::onData(const boost::shared_ptr<const Interest>& interest,
			const boost::shared_ptr<Data>& data)
{
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
		}
		else
			LogWarnC << "received invalid data " << data->getName() << std::endl;
	}
	else
		LogWarnC << "received data with bad name: " << data->getName() << std::endl;
}

void SegmentController::onTimeout(const boost::shared_ptr<const Interest>& interest)
{
	NamespaceInfo info;

	if (NameComponents::extractInfo(interest->getName(), info))
	{
		LogTraceC << "received timeout for " << interest->getName() << std::endl;

		{
			boost::lock_guard<boost::mutex> scopedLock(mutex_);
			for (auto& o:observers_) o->segmentRequestTimeout(info);
		}
	}
	else
		LogWarnC << "received timeout for badly named Interest " << interest->getName() << std::endl;
}
