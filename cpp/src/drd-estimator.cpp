// 
// drd-estimator.cpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/thread/lock_guard.hpp>
#include <boost/make_shared.hpp>

#include "drd-estimator.hpp"

using namespace ndnrtc;
using namespace estimators;

DrdEstimator::DrdEstimator(unsigned int initialEstimationMs, unsigned int windowMs):
initialEstimation_(initialEstimationMs),
windowSize_(windowMs),
cachedDrd_(Average(boost::make_shared<TimeWindow>(windowMs))),
originalDrd_(Average(boost::make_shared<TimeWindow>(windowMs))),
generationDelay_(Average(boost::make_shared<TimeWindow>(windowMs))),
latest_(&originalDrd_)
{}

void
DrdEstimator::newValue(double drd, bool isOriginal, double dGen)
{
	double oldValue = (isOriginal ? originalDrd_.value() : cachedDrd_.value());

	if (dGen > 0) generationDelay_.newValue(dGen);

	if (isOriginal) 
		originalDrd_.newValue(drd);
	else 
		cachedDrd_.newValue(drd);

	latest_ = (isOriginal ? &originalDrd_ : &cachedDrd_);
	
	double newValue = (isOriginal ? originalDrd_.value() : cachedDrd_.value());
    double deviation = (isOriginal ? originalDrd_.deviation() : cachedDrd_.deviation());
	
	if (oldValue != newValue)
	{
		boost::lock_guard<boost::mutex> scopedLock(mutex_);
		for (auto& o:observers_) {
			if (isOriginal)
				o->onOriginalDrdUpdate(newValue, deviation);
			else
				o->onCachedDrdUpdate(newValue, deviation);
			o->onDrdUpdate();
		}
	}
}

double
DrdEstimator::getCachedEstimation() const
{ return  cachedDrd_.count() ? cachedDrd_.value() : (double)initialEstimation_; }

double 
DrdEstimator::getOriginalEstimation() const
{ return originalDrd_.count() ? originalDrd_.value() : (double)initialEstimation_; }

void
DrdEstimator::reset()
{
	cachedDrd_ = Average(boost::make_shared<TimeWindow>(windowSize_));
	originalDrd_ = Average(boost::make_shared<TimeWindow>(windowSize_));
}

void DrdEstimator::attach(IDrdEstimatorObserver* o)
{
    if (o)
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.push_back(o);
    }
}

void DrdEstimator::detach(IDrdEstimatorObserver* o)
{
    std::vector<IDrdEstimatorObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), o);
    if (it != observers_.end())
    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}
