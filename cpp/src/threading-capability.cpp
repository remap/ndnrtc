//
//  threading-capability.cpp
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "threading-capability.hpp"

using namespace ndnrtc;
using namespace boost;

//******************************************************************************
void ThreadingCapability::startMyThread()
{
    threadWork_.reset(new boost::asio::io_service::work(ioService_));
    thread_ = thread([this](){
        ioService_.run();
    });
}

void ThreadingCapability::stopMyThread()
{
    threadWork_.reset();
    ioService_.stop();
    thread_.try_join_for(chrono::milliseconds(500));
}

void ThreadingCapability::dispatchOnMyThread(boost::function<void(void)> dispatchBlock)
{
    if (threadWork_.get())
    {
        if (this_thread::get_id() == thread_.get_id())
            dispatchBlock();
        else
            ioService_.dispatch([=]{
                dispatchBlock();
            });
    }
}

void ThreadingCapability::performOnMyThread(boost::function<void(void)> dispatchBlock)
{
    if (threadWork_.get())
    {
        if (this_thread::get_id() == thread_.get_id())
            dispatchBlock();
        else
        {
            mutex m;
            unique_lock<mutex> lock(m);
            condition_variable isDone;
            // doneFlag is needed to prevent situations where the block passed to ioService_
            // finishes before current thread reaches isDone.wait() call 
            boost::atomic<bool> doneFlag(false);
            
            ioService_.dispatch([dispatchBlock,&isDone, &doneFlag]{
                dispatchBlock();
                doneFlag = true;
                isDone.notify_one();
            });

            isDone.wait(lock, [&doneFlag](){ return doneFlag.load(); });
        }
    }
}