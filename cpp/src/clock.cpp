// 
// clock.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/chrono.hpp>

#include "clock.hpp"

using namespace boost::chrono;
using namespace ndnrtc;

namespace ndnrtc {
	namespace clock {
		// monotonic clock
		int64_t millisecondTimestamp()
		{
			milliseconds msec = duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
			return msec.count();
		};

		// monotonic clock
		int64_t microsecondTimestamp()
		{
			microseconds usec = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
			return usec.count();
		};

		// monotonic clock
		int64_t nanosecondTimestamp()
		{
			boost::chrono::nanoseconds nsec = steady_clock::now().time_since_epoch();
			return nsec.count();
		};

		// system clock
		double unixTimestamp()
		{
			auto now = system_clock::now().time_since_epoch();
			boost::chrono::duration<double> sec = now;
			return sec.count();
		}

		// system clock
		int64_t millisecSinceEpoch()
		{
			milliseconds msec = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
			return msec.count();
		}
	}
}