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
segNum_(Average(boost::make_shared<SampleWindow>(30))),
segSize_(Average(boost::make_shared<SampleWindow>(30)))
{}

SampleEstimator::Estimators::~_Estimators()
{}

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
	SampleClass st = segment->isDelta() ? SampleClass::Delta : SampleClass::Key;
	SegmentClass dt = segment->isParity() ? SegmentClass::Parity : SegmentClass::Data;

	estimators_[std::make_pair(st,dt)].segNum_.newValue(segment->getSlicesNum());
	estimators_[std::make_pair(st,dt)].segSize_.newValue(segment->getData()->getContent().size());
}

void 
SampleEstimator::reset()
{
	estimators_ = boost::assign::map_list_of
		( std::make_pair(SampleClass::Delta, SegmentClass::Data), Estimators())
		( std::make_pair(SampleClass::Delta, SegmentClass::Parity), Estimators())
		( std::make_pair(SampleClass::Key, SegmentClass::Data), Estimators())
		( std::make_pair(SampleClass::Key, SegmentClass::Parity), Estimators());
}

double 
SampleEstimator::getSegmentNumberEstimation(SampleClass st, SegmentClass dt)
{
	return estimators_[std::make_pair(st,dt)].segNum_.value();
}

double
SampleEstimator::getSegmentSizeEstimation(SampleClass st, SegmentClass dt)
{
	return estimators_[std::make_pair(st,dt)].segSize_.value();
}

#pragma mark - private
