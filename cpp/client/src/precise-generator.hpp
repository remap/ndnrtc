// 
// precise-generator.h
//
//  Created by Peter Gusev on 19 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __precise_generator_h__
#define __precise_generator_h__

#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>

#define STEADY_TIMER

#ifndef OS_DARWIN

#include <functional>
#include <iostream>
#include <chrono>
#include <boost/asio.hpp>

namespace lib_bind=std;
namespace lib_fun=std;
namespace lib_chrono=std::chrono;

using namespace std::placeholders;

#else

#include <boost/function.hpp>
#include <boost/chrono.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

namespace lib_bind=boost;
namespace lib_fun=boost;
namespace lib_chrono=boost::chrono;

#endif

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

//******************************************************************************

class PreciseGeneratorImpl;

class PreciseGenerator {
public:
    typedef lib_fun::function<void()> Task;
    PreciseGenerator(boost::asio::io_service& io, const double& ratePerSec,
                     const Task& task);
    ~PreciseGenerator();
    
    void start();
    void stop();
    bool isRunning();
    long long getFireCount();
    double getMeanProcessingOverheadNs();
    double getMeanTaskTimeNs();
    
private:
    boost::shared_ptr<PreciseGeneratorImpl> pimpl_;
};

#endif