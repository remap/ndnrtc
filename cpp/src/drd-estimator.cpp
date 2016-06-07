// 
// drd-estimator.cpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "drd-estimator.h"

using namespace ndnrtc;
using namespace estimators;

DrdEstimator::DrdEstimator(unsigned int initialEstimationMs, unsigned int windowMs):
initialEstimation_(initialEstimationMs),
windowSize_(windowMs),
cachedDrd_(Average(boost::make_shared<TimeWindow>(windowMs))),
originalDrd_(Average(boost::make_shared<TimeWindow>(windowMs)))
{}

void
DrdEstimator::newValue(double drd, bool isOriginal)
{
	if (isOriginal) originalDrd_.newValue(drd);
	else cachedDrd_.newValue(drd);
}

double
DrdEstimator::getCachedEstimation()
{ return  cachedDrd_.count() ? cachedDrd_.value() : (double)initialEstimation_; }

double 
DrdEstimator::getOriginalEstimation()
{ return originalDrd_.count() ? originalDrd_.value() : (double)initialEstimation_; }

void
DrdEstimator::reset()
{
	cachedDrd_ = Average(boost::make_shared<TimeWindow>(windowSize_));
	originalDrd_ = Average(boost::make_shared<TimeWindow>(windowSize_));
}