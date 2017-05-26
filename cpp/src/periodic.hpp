// 
// periodic.hpp
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __periodic_h__
#define __periodic_h__

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

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
		Periodic(boost::asio::io_service& io);
		virtual ~Periodic();

		/**
		 * This sets up periodic invocation
		 * @param intervalMs Interval in milliseconds to schedule timer for
		 * @param callback Callback to call periodically
		 */
		void setupInvocation(unsigned int intervalMs, 
			boost::function<unsigned int(void)> callback);

		/**
		 * This cancels periodic invocation
		 */
		void cancelInvocation();

		/**
		 * This returns true if invocation was set up. False otherwise.
		 */
		bool isPeriodicInvocationSet() const;

	private:
		boost::shared_ptr<PeriodicImpl> pimpl_;
	};
}

#endif