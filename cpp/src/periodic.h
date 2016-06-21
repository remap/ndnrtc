// 
// periodic.h
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __periodic_h__
#define __periodic_h__

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

namespace ndnrtc 
{
	class PeriodicImpl;

	/**
	 * This is a helper class that implements thread-safe periodic invocation.
	 * One shall derive form this class and provide implementation for doWork()
	 * method.
	 */
	class Periodic {
	public:
		Periodic(boost::asio::io_service& io, unsigned int periodMs);
		virtual ~Periodic();

		/**
		 * Called repeatedly, each periodMs milliseconds. Client can alter
		 * next time period by returning a number of milliseconds other than 
		 * periodMs.
		 * @return Time interval in milliseconds for next call. If zero - cancels
		 * periodic invocations. 
		 */
		virtual unsigned int periodicInvocation() = 0;

		/**
		 * If invocation was stopped (by returning 0 in periodicInvocation), this
		 * schedules timer again.
		 * @param intervalMs Interval in milliseconds to schedule timer for
		 */
		void setupInvocation(unsigned int intervalMs);

	private:
		boost::shared_ptr<PeriodicImpl> pimpl_;
	};
}

#endif