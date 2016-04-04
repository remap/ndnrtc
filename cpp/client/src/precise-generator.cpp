// 
// precise-generator.cpp
//
//  Created by Peter Gusev on 19 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

// #define DEBUG_PG
#include "precise-generator.h"

using namespace std;

PreciseGenerator::PreciseGenerator(boost::asio::io_service& io, const double& ratePerSec, 
	const Task& task):
io_(io), timer_(io), rate_(ratePerSec), task_(task),
lastIterIntervalMs_(0), meanProcOverheadNs_(0), meanTaskTimeNs_(0)
#ifdef STEADY_TIMER
, lagNs_(0), lastTaskDurationNs_(0)
#endif
{}

void PreciseGenerator::start()
{
	fireCount_ = 0;
	isRunning_ = true;
	setupTimer();
}

void PreciseGenerator::stop()
{
	isRunning_ = false;
	timer_.cancel();
}

//******************************************************************************
void PreciseGenerator::setupTimer()
{
	time_point_type lastIterStart = iterStart_;
	iterStart_ = now_time();
	ms interval = getAdjustedInterval(lastIterStart, iterStart_, 
		lastIterIntervalMs_, lastTaskDurationNs_, rate_);
	timer_.expires_from_now(interval);
	timer_.async_wait(lib_bind::bind(&PreciseGenerator::onFire, this, _1));
	lastIterIntervalMs_ = interval;
}

void PreciseGenerator::onFire(const boost::system::error_code& code)
{
	if (code != boost::asio::error::operation_aborted && isRunning_)
	{
		time_point_type tstart = now_time();
		task_();
		time_point_type tend = now_time();
		fireCount_++;

		lastTaskDurationNs_ = duration_cast_ns((tend-tstart));
		meanTaskTimeNs_ += (double)(duration_cast_double_ns(lastTaskDurationNs_) - meanTaskTimeNs_)/(double)fireCount_;

		setupTimer();
	}
}

ms PreciseGenerator::getAdjustedInterval(const time_point_type& lastIterStart,
	const time_point_type& thisIterStart,
	const ms lastIterIntervalMs,
	const ns lastTaskDurationNs,
	const double& rate)
{
	ns originalInterval = ns((int)(1000000000./rate));
	ms adjustedIntervalMs = duration_cast_ms(originalInterval);
	ms originalIntervalMs = duration_cast_ms(originalInterval);
	ns diff = duration_cast_ns(originalInterval - duration_cast_ns(duration_cast_ms((originalInterval))));

	lagNs_ -= diff;
	
	#ifdef DEBUG_PG
	cout << "remainder " << print_duration_ns(diff) << endl;
	#endif

	if (!is_time_point_zero(lastIterStart))
	{
		ns lastIterDurationNs = duration_cast_ns((thisIterStart-lastIterStart));
		ns iterLag = duration_cast_ns(lastIterDurationNs-lastIterIntervalMs);

		lagNs_ += iterLag;
		meanProcOverheadNs_ += (double)(duration_cast_double_ns(iterLag-lastTaskDurationNs) - meanProcOverheadNs_)/(double)fireCount_;

		#ifdef DEBUG_PG
		cout << "last iter time " << print_duration_ns(lastIterDurationNs) 
			<< " (should " << print_duration_ns(lastIterIntervalMs) << ", "
			<< print_duration_ns(iterLag) << " lag, " 
			<< print_duration_ns(lastTaskDurationNs) << " task)" << endl;
		cout << "accum lag " << print_duration_ns(lagNs_) << endl;
		#endif

		if (lagNs_ > originalIntervalMs)
		{
			lagNs_ -= originalIntervalMs;
			adjustedIntervalMs = ms(0);
		
		}
		else if (abs(duration_count(duration_cast_ms(lagNs_))) > 0)
		{
			adjustedIntervalMs = duration_cast_ms(originalIntervalMs - duration_cast_ms(lagNs_));
			lagNs_ -= duration_cast_ms(lagNs_);
		}
	}

	#ifdef DEBUG_PG
	cout << "interval " << print_duration_ns(adjustedIntervalMs) << endl;
	cout << "adjusted lag " << print_duration_ns(lagNs_) << endl << "-" << endl;
	#endif

	return adjustedIntervalMs;
}