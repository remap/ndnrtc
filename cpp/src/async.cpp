// 
// async.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "async.hpp"

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

namespace ndnrtc
{
	namespace async {

		void dispatchAsync(boost::asio::io_service& io,
			boost::function<void(void)> dispatchBlock,
			boost::function<void(void)> onCompletion)
		{
			io.dispatch([=]{
				dispatchBlock();
				if (onCompletion) onCompletion();
			});
		}
		
		void dispatchSync(boost::asio::io_service& io,
			boost::function<void(void)> dispatchBlock,
			boost::function<void(void)> onCompletion)
		{
			boost::mutex m;
			boost::unique_lock<boost::mutex> lock(m);
			boost::condition_variable isDone;
            boost::atomic<bool> doneFlag(false);

			io.dispatch([&isDone, dispatchBlock, onCompletion, &doneFlag]{
				dispatchBlock();
				if (onCompletion) onCompletion();
                doneFlag = true;
				isDone.notify_one();
			});

            isDone.wait(lock, [&doneFlag](){ return doneFlag.load(); });
		}
	}
}