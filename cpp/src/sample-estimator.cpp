// 
// sample-estimator.cpp
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "sample-estimator.hpp"
#include <boost/assign.hpp>

#include "estimators.hpp"
#include "frame-data.hpp"
#include "statistics.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace estimators;

//******************************************************************************
SampleEstimator::Estimators::_Estimators():
segNum_(Average(boost::make_shared<SampleWindow>(30))),
segSize_(Average(boost::make_shared<SampleWindow>(30)))
{}

SampleEstimator::Estimators::~_Estimators()
{}

//******************************************************************************
SampleEstimator::SampleEstimator(const boost::shared_ptr<statistics::StatisticsStorage>& storage):
sstorage_(storage)
{
	reset();
}

SampleEstimator::~SampleEstimator()
{}

void
SampleEstimator::bootstrapSegmentNumber(double value, SampleClass st, SegmentClass dt)
{
    estimators_[std::make_pair(st,dt)].segNum_.newValue(value);
}

void SampleEstimator::bootstrapSegmentSize(double value, SampleClass st, SegmentClass dt)
{
    estimators_[std::make_pair(st,dt)].segSize_.newValue(value);
}

void 
SampleEstimator::segmentArrived(const boost::shared_ptr<WireSegment>& segment)
{
    if (segment->getSampleClass() == SampleClass::Key ||
        segment->getSampleClass() == SampleClass::Delta)
    {
        SampleClass st = segment->getSampleClass();
        SegmentClass dt = segment->getSegmentClass();
        
        estimators_[std::make_pair(st,dt)].segNum_.newValue(segment->getSlicesNum());
        estimators_[std::make_pair(st,dt)].segSize_.newValue(segment->getData()->getContent().size());
        
        if (st == SampleClass::Delta)
        {
            if (dt == SegmentClass::Data)
                (*sstorage_)[Indicator::SegmentsDeltaAvgNum] = segment->getSlicesNum();
            else
                (*sstorage_)[Indicator::SegmentsDeltaParityAvgNum] = segment->getSlicesNum();
        }
        else
        {
            if (dt == SegmentClass::Data)
                (*sstorage_)[Indicator::SegmentsKeyAvgNum] = segment->getSlicesNum();
            else
                (*sstorage_)[Indicator::SegmentsKeyParityAvgNum] = segment->getSlicesNum();
        }
    }
}

void
SampleEstimator::reset()
{
	const EstimatorMap m = boost::assign::map_list_of
		( std::make_pair(SampleClass::Delta, SegmentClass::Data), Estimators())
		( std::make_pair(SampleClass::Delta, SegmentClass::Parity), Estimators())
		( std::make_pair(SampleClass::Key, SegmentClass::Data), Estimators())
		( std::make_pair(SampleClass::Key, SegmentClass::Parity), Estimators());
	estimators_ = m;    

    (*sstorage_)[Indicator::SegmentsDeltaAvgNum] = 0;
    (*sstorage_)[Indicator::SegmentsDeltaParityAvgNum] = 0;
    (*sstorage_)[Indicator::SegmentsKeyAvgNum] = 0;
    (*sstorage_)[Indicator::SegmentsKeyParityAvgNum] = 0;
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
