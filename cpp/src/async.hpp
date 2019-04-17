// 
// async.hpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __async_h__
#define __async_h__

#include <boost/asio.hpp>

namespace ndnrtc {
	namespace async {
		void dispatchAsync(boost::asio::io_service& io,
			std::function<void(void)> dispatchBlock,
			std::function<void(void)> onCompletion = std::function<void(void)>());
		
		void dispatchSync(boost::asio::io_service& io,
			std::function<void(void)> dispatchBlock,
			std::function<void(void)> onCompletion = std::function<void(void)>());
	}
}

#endif
