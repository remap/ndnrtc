// 
// estimators.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <cstdlib>
#include <vector>
#include <cmath>

#include "estimators.hpp"
#include "clock.hpp"

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

void
SampleWindow::cut(std::deque<double>& samples)
{
    while (samples.size() >= nSamples_) samples.pop_front();
}

TimeWindow::TimeWindow(unsigned int milliseconds):
milliseconds_(milliseconds), lastReach_(0)
{
	assert(milliseconds_);
}

bool
TimeWindow::isLimitReached()
{
	int64_t now = clock::millisecondTimestamp();
    if (lastReach_ == 0) lastReach_ = now;
    
	if (now-lastReach_ > milliseconds_)
	{
		lastReach_ += milliseconds_;
		return true;
	}

	return false;
}

void
TimeWindow::cut(std::deque<double>& samples)
{
    double now = (double)clock::millisecondTimestamp();
    while (samples.front() < now-milliseconds_) samples.pop_front();
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
run_(false)
{}

void
FreqMeter::newValue(double value)
{
    int64_t now = clock::millisecondTimestamp();
	nValues_++;
    samples_.push_back((double)now);

	if (window_->isLimitReached())
        run_ = true;
    
    if (run_) window_->cut(samples_);
    if (samples_.size() > 1)
        value_ = 1000.*(double)samples_.size()/(samples_.back()-samples_.front());
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
