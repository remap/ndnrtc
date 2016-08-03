// 
// interest-control.cpp
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "interest-control.h"
#include <algorithm>

#include "frame-data.h"
#include "name-components.h"
#include "estimators.h"

using namespace ndnrtc;
using namespace ndnrtc::statistics;

const unsigned int InterestControl::MinPipelineSize = 3;

void 
InterestControl::StrategyDefault::getLimits(double rate, 
	const estimators::Average& drd, 
	unsigned int& lowerLimit, unsigned int& upperLimit)
{
	double d = drd.value() + 
		4*drd.deviation();
	double targetSamplePeriod = 1000./rate;
	int interestDemand = (int)(ceil(d/targetSamplePeriod));
	
    if (interestDemand < InterestControl::MinPipelineSize)
        interestDemand = InterestControl::MinPipelineSize;

	lowerLimit = interestDemand;
	upperLimit = interestDemand*8;
}

int
InterestControl::StrategyDefault::burst(unsigned int currentLimit, 
				unsigned int lowerLimit, unsigned int upperLimit)
{
	return (int)ceil((double)currentLimit/2.);
}

int 
InterestControl::StrategyDefault::withhold(unsigned int currentLimit, 
	unsigned int lowerLimit, unsigned int upperLimit)
{
	return -(int)round((double)(currentLimit-lowerLimit)/2.);
}

//******************************************************************************
InterestControl::InterestControl(const boost::shared_ptr<DrdEstimator>& drdEstimator,
    const boost::shared_ptr<statistics::StatisticsStorage>& storage,
	boost::shared_ptr<IStrategy> strategy):
initialized_(false), 
lowerLimit_(InterestControl::MinPipelineSize),
limit_(InterestControl::MinPipelineSize),
upperLimit_(10*InterestControl::MinPipelineSize), pipeline_(0),
drdEstimator_(drdEstimator), sstorage_(storage), strategy_(strategy),
targetRate_(0.)
{
	description_ = "interest-control";
}

InterestControl::~InterestControl()
{
}

void
InterestControl::reset()
{
	initialized_ = false;
	limitSet_ = false;
	pipeline_ = 0;
	lowerLimit_ = InterestControl::MinPipelineSize;
	limit_ = InterestControl::MinPipelineSize;
	upperLimit_ = 30;
    
    (*sstorage_)[Indicator::DW] = limit_;
    (*sstorage_)[Indicator::W] = pipeline_;
}

bool
InterestControl::decrement()
{
	pipeline_--;
    assert(pipeline_ >= 0);
    
	LogTraceC << "▼dec " << snapshot() << std::endl;
    (*sstorage_)[Indicator::W] = pipeline_;
	return true;
}

bool
InterestControl::increment()
{
	if (pipeline_ >= (int)limit_)
		return false;

	pipeline_++;
    
	LogTraceC << "▲inc " << snapshot() << std::endl;
    (*sstorage_)[Indicator::W] = pipeline_;
	return true;
}

bool
InterestControl::burst()
{
	if (initialized_)
	{
		int d = strategy_->burst(limit_, lowerLimit_, upperLimit_);

		changeLimitTo(limit_+d);
		LogDebugC << "burst by " << d << " " << snapshot() << std::endl;

		return true;
	}

	return false;
}

bool
InterestControl::withhold()
{
	if (initialized_)
	{
		int d = strategy_->withhold(limit_, lowerLimit_, upperLimit_);

		if (d != 0)
			changeLimitTo(limit_+d);
		
		LogDebugC << "withhold by " << -d << " " << snapshot() << std::endl;

		return (d!=0);
	}

	return false;
}

void 
InterestControl::markLowerLimit(unsigned int lowerLimit) 
{ 
	limitSet_ = true; 
	lowerLimit_ = lowerLimit; 
	setLimits();
}

void 
InterestControl::onDrdUpdate()
{
	if (initialized_)
		setLimits();
}

void
InterestControl::targetRateUpdate(double rate)
{ 
	targetRate_ = rate; 
	initialized_ = true;
	setLimits();
}

void 
InterestControl::setLimits()
{
	unsigned int newLower = 0, newUpper = 0;
	strategy_->getLimits(targetRate_, drdEstimator_->getLatestUpdatedAverage(),
		newLower, newUpper);

	if (lowerLimit_ != newLower ||
		upperLimit_ != newUpper)
	{
		if (!limitSet_ || newLower > lowerLimit_) lowerLimit_ = newLower;
		upperLimit_ = newUpper;

		if (limit_ < lowerLimit_)
			changeLimitTo(lowerLimit_);
        
        LogDebugC << "DRD orig: " << drdEstimator_->getOriginalEstimation()
            << " cach: " << drdEstimator_->getCachedEstimation()
            << ", set limits " << snapshot() << std::endl;
	}
}

void
InterestControl::changeLimitTo(unsigned int newLimit)
{
	if (newLimit < lowerLimit_)
	{
		LogWarnC << "can't set limit (" 
			<< newLimit
			<< ") lower than lower limit (" 
			<< lowerLimit_ << ")" << std::endl;

		limit_ = lowerLimit_;
	}
	else if (newLimit > upperLimit_)
	{
		LogWarnC << "can't set limit ("
			<< newLimit 
			<< ") higher than upper limit ("
			<< upperLimit_ << ")" << std::endl;
		limit_ = upperLimit_;
	}
	else
		limit_ = newLimit;
    
    (*sstorage_)[Indicator::DW] = limit_;
}

std::string
InterestControl::snapshot() const
{
	std::stringstream ss;
	ss << lowerLimit_  << "-" << upperLimit_ << "[";
    for (int i = 1;
         i <= fmax(limit_, pipeline_);
         ++i)
    {
        if (i > limit_)
            ss << "⥣";
        else
        {
            if (i <= pipeline_)
                ss << "⬆︎";
            else
                ss << "◻︎";
        }
    }
	ss << "]" << pipeline_ << "-" << limit_ << " (" << room() <<")";

	return ss.str();
}
