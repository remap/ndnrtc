// 
// estimators.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <cstdlib>
#include <vector>
#include <cmath>

#include "estimators.h"
#include "clock.h"

using namespace ndnrtc::estimators;
using namespace ndnrtc::clock;
using namespace std;

//******************************************************************************
bool 
SampleWindow::isLimitReached()
{
	remaining_--;
	if (remaining_ == 0) remaining_ = nSamples_;
	return (remaining_ == nSamples_);
}

TimeWindow::TimeWindow(unsigned int milliseconds):
milliseconds_(milliseconds), lastReach_(clock::millisecondTimestamp())
{
	assert(milliseconds_);
}

bool
TimeWindow::isLimitReached()
{
	int64_t now = clock::millisecondTimestamp();
	if (now-lastReach_ >= milliseconds_) 
	{
		lastReach_ += milliseconds_;
		return true;
	}

	return false;
}

//******************************************************************************
Average::Average(boost::shared_ptr<IEstimatorWindow> window):
Estimator(window), accumulatedSum_(0.), variance_(0.), limitReached_(false)
{
}

void
Average::newValue(double value)
{
	bool windowLimit = window_->isLimitReached();
	nValues_++;
	samples_.push_back(value);

	if (limitReached_)
	{
		accumulatedSum_ += value - samples_.front();
		samples_.pop_front();
	}
	else
	{
		limitReached_ = windowLimit;
		accumulatedSum_ += value;
	}

	value_ = accumulatedSum_/samples_.size();

	// re-calculate deviation every window
	if (windowLimit)
	{
		variance_ = 0.;
		for (auto& v:samples_) variance_ += (v-value_)*(v-value_);
		variance_ = variance_/samples_.size();
	}
}

//******************************************************************************
FreqMeter::FreqMeter(boost::shared_ptr<IEstimatorWindow> window):Estimator(window),
ts_(0), nCalls_(0)
{}

void
FreqMeter::newValue(double value)
{
	nValues_++;
	nCalls_++;

	if (window_->isLimitReached())
	{
		int64_t now = clock::millisecondTimestamp();

		if (ts_ > 0)
		{
			value_ = 1000.*(double)nCalls_/(double)(now-ts_);
			nCalls_ = 0;
		}

		ts_ = now;
	}
}

Filter::Filter(double smoothing):smoothing_(smoothing), value_(0){}

void
Filter::newValue(double value)
{
	if (value_ == 0)
		value_ = value;
	else
		value_ += (value-value_)*smoothing_;
}
