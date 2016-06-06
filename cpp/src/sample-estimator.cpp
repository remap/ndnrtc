// 
// sample-estimator.cpp
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "sample-estimator.h"
#include <boost/assign.hpp>

#include "estimators.h"
#include "frame-data.h"

using namespace ndnrtc;
using namespace estimators;

//******************************************************************************
SampleEstimator::Estimators::_Estimators():
segNum_(setupSlidingAverageEstimator(30)),
segSize_(setupSlidingAverageEstimator(30))
{}

SampleEstimator::Estimators::~_Estimators()
{
	resetSlidingAverageEstimator(segNum_);
	resetSlidingAverageEstimator(segSize_);
}

//******************************************************************************
SampleEstimator::SampleEstimator()
{
	reset();
}

SampleEstimator::~SampleEstimator()
{}

void 
SampleEstimator::segmentArrived(const boost::shared_ptr<WireSegment>& segment)
{
	SampleType st = segment->isDelta() ? Delta : Key;
	DataType dt = segment->isParity() ? Parity : Data;
	slidingAverageEstimatorNewValue(estimators_[std::make_pair(st,dt)].segNum_, (double)segment->getSlicesNum());
	slidingAverageEstimatorNewValue(estimators_[std::make_pair(st,dt)].segSize_, (double)segment->getData()->getContent().size());
}

void 
SampleEstimator::reset()
{
	estimators_ = boost::assign::map_list_of
		( std::make_pair(SampleEstimator::Delta, SampleEstimator::Data), Estimators())
		( std::make_pair(SampleEstimator::Delta, SampleEstimator::Parity), Estimators())
		( std::make_pair(SampleEstimator::Key, SampleEstimator::Data), Estimators())
		( std::make_pair(SampleEstimator::Key, SampleEstimator::Parity), Estimators());
}

double 
SampleEstimator::getSegmentNumberEstimation(SampleType st, DataType dt)
{
	return currentSlidingAverageValue(estimators_[std::make_pair(st,dt)].segNum_);
}

double
SampleEstimator::getSegmentSizeEstimation(SampleType st, DataType dt)
{
	return currentSlidingAverageValue(estimators_[std::make_pair(st,dt)].segSize_);
}

#pragma mark - private
