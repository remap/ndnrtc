//
// clock.hpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __clock_h__
#define __clock_h__

#include <stdlib.h>
#include "proto/ndnrtc.pb.h"

namespace ndnrtc {
	namespace clock {
		/**
		 * Returns millisecond timestamp (monotonic clock)
		 */
		int64_t millisecondTimestamp();

		/**
		 * Returns microsecond timestamp (monotonic clock)
		 */
		int64_t microsecondTimestamp();

		/**
		 * Returns nanosecond timestamp (monotonic clock)
		 */
		int64_t nanosecondTimestamp();

		/**
		 * Returns unix timestamp in seconds since epoch (system clock)
		 */
		double unixTimestamp();

		/**
		 * Returns unix timestamp in milliseconds since epoch (system clock)
		 */
		int64_t millisecSinceEpoch();

		/**
		 * Returns Unix Timestamp as a google::protobuf::Timestamp object
		 */
		google::protobuf::Timestamp protobufMillisecondTimestamp();
	}
}

#endif
