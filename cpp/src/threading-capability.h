//
//  threading-capability.h
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>

namespace ndnrtc {
    namespace new_api
    {
        class ThreadingCapability {
        protected:
            ThreadingCapability(){}
            ~ThreadingCapability(){}
            
            void startMyThread();
            void stopMyThread();
            // asynchronous
            void dispatchOnMyThread(boost::function<void(void)> dispatchBlock);
            // synchronous
            void performOnMyThread(boost::function<void(void)> dispatchBlock);
            
        private:
            boost::shared_ptr<boost::asio::io_service::work> threadWork_;
            boost::asio::io_service ioService_;
            boost::thread thread_;
        };
    }
}