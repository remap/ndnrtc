// 
// precise-generator.cpp
//
//  Created by Peter Gusev on 19 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

// #define DEBUG_PG
#include "precise-generator.hpp"
#include <boost/make_shared.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#define STEADY_TIMER

#ifdef STEADY_TIMER

#include <boost/asio/steady_timer.hpp>
typedef boost::asio::steady_timer timer_type;
typedef lib_chrono::milliseconds ms;
typedef lib_chrono::nanoseconds ns;
typedef lib_chrono::high_resolution_clock::time_point time_point_type;
typedef lib_chrono::high_resolution_clock::duration duration_type;
#define duration_cast_ms(a) (lib_chrono::duration_cast<ms>(a))
#define duration_cast_ns(a) (lib_chrono::duration_cast<ns>(a))
#define duration_cast_double_ns(a) double(lib_chrono::duration_cast<ns>(a).count())
#define duration_count(a) (a).count()
#define now_time() (lib_chrono::high_resolution_clock::now())
#define is_time_point_zero(pt) (pt.time_since_epoch() == lib_chrono::high_resolution_clock::duration::zero())
#define print_duration_ns(d) (d.count())

#else // deadline timer

// requires posix_time
#include <boost/asio/deadline_timer.hpp>
typedef boost::asio::deadline_timer timer_type;
typedef boost::posix_time::ptime time_point_type;
typedef boost::posix_time::milliseconds ms;
typedef boost::posix_time::nanoseconds ns;
typedef boost::posix_time::time_duration duration_type;
#define duration_cast_ms(a) ms((a).total_milliseconds())
#define duration_cast_ns(a) ns((a).total_nanoseconds())
#define duration_cast_double_ns(a) double((a).total_nanoseconds())
#define duration_count(a) (a).ticks()
#define now_time() (boost::posix_time::microsec_clock::local_time())
#define is_time_point_zero(pt) (pt == boost::posix_time::not_a_date_time)
#define print_duration_ns(d) (d.total_nanoseconds())

#endif

using namespace std;
using namespace std::placeholders;

class PreciseGeneratorImpl : public boost::enable_shared_from_this<PreciseGeneratorImpl>
{
public:
    PreciseGeneratorImpl(boost::asio::io_service& io, const double& ratePerSec,
                         const PreciseGenerator::Task& task);
    
    void start();
    void stop();
    bool isRunning(){ return isRunning_; }
    long long getFireCount() { return fireCount_; }
    double getMeanProcessingOverheadNs() { return (meanProcOverheadNs_); }// - meanTaskTimeNs_); }
    double getMeanTaskTimeNs() { return meanTaskTimeNs_; }
    
    boost::recursive_mutex setupMutex_;
    boost::atomic<bool> isRunning_;
    long long fireCount_;
    double meanProcOverheadNs_, meanTaskTimeNs_;
    double rate_;
    PreciseGenerator::Task task_;
    boost::asio::io_service& io_;
    timer_type timer_;
    
    time_point_type iterStart_;
    duration_type lagNs_, lastTaskDurationNs_;
    ms lastIterIntervalMs_;
    
    void setupTimer();
    void onFire(const boost::system::error_code& code);
    ms getAdjustedInterval(const time_point_type& lastIterStart,
                           const time_point_type& thisIterStart,
                           const ms lastIterIntervalMs,
                           const ns lastTaskDurationNs,
                           const double& rate);
};

//******************************************************************************
PreciseGenerator::PreciseGenerator(boost::asio::io_service& io, const double& ratePerSec,
                                   const Task& task):
pimpl_(boost::make_shared<PreciseGeneratorImpl>(io, ratePerSec, task)){}
PreciseGenerator::~PreciseGenerator(){ pimpl_->stop(); }

void PreciseGenerator::start(){ pimpl_->start(); }
void PreciseGenerator::stop(){ pimpl_->stop(); }
bool PreciseGenerator::isRunning(){ return pimpl_->isRunning(); }
long long PreciseGenerator::getFireCount(){ return pimpl_->getFireCount(); }
double PreciseGenerator::getMeanProcessingOverheadNs(){ return pimpl_->getMeanProcessingOverheadNs(); }
double PreciseGenerator::getMeanTaskTimeNs(){ return pimpl_->getMeanTaskTimeNs(); }

//******************************************************************************
PreciseGeneratorImpl::PreciseGeneratorImpl(boost::asio::io_service& io, const double& ratePerSec,
                                           const PreciseGenerator::Task& task):
io_(io), timer_(io), rate_(ratePerSec), task_(task),
lastIterIntervalMs_(0), meanProcOverheadNs_(0), meanTaskTimeNs_(0)
#ifdef STEADY_TIMER
, lagNs_(0), lastTaskDurationNs_(0)
#endif
{}

void PreciseGeneratorImpl::start()
{
	fireCount_ = 0;
	isRunning_ = true;
	setupTimer();
}

void PreciseGeneratorImpl::stop()
{
    if (isRunning_)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(setupMutex_);
        isRunning_ = false;
        timer_.cancel();
    }
}

void PreciseGeneratorImpl::setupTimer()
{
	time_point_type lastIterStart = iterStart_;
	iterStart_ = now_time();
	ms interval = getAdjustedInterval(lastIterStart, iterStart_, 
		lastIterIntervalMs_, lastTaskDurationNs_, rate_);
	timer_.expires_from_now(interval);
#ifdef OS_DARWIN
	timer_.async_wait(lib_bind::bind(&PreciseGeneratorImpl::onFire, shared_from_this(), _1));
#else
	timer_.async_wait(lib_bind::bind(&PreciseGeneratorImpl::onFire, shared_from_this(), boost::arg<1>()));
#endif
	lastIterIntervalMs_ = interval;
}

void PreciseGeneratorImpl::onFire(const boost::system::error_code& code)
{
    if (!code)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(setupMutex_);
        
        if (isRunning_)
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
}

ms PreciseGeneratorImpl::getAdjustedInterval(const time_point_type& lastIterStart,
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
