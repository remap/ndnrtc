//
// async.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "async.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ndnrtc
{
	namespace async {

		void dispatchAsync(boost::asio::io_service& io,
			std::function<void(void)> dispatchBlock,
			std::function<void(void)> onCompletion)
		{
			io.dispatch([=]{
				dispatchBlock();
				if (onCompletion) onCompletion();
			});
		}

		void dispatchSync(boost::asio::io_service& io,
			std::function<void(void)> dispatchBlock,
			std::function<void(void)> onCompletion)
		{
			std::mutex m;
			std::unique_lock<std::mutex> lock(m);
			std::condition_variable isDone;
			std::atomic<bool> doneFlag(false);

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
