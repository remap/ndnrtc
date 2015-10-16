//
//  threading-capability.cpp
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "threading-capability.h"

using namespace ndnrtc::new_api;
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
            
            ioService_.dispatch([dispatchBlock,&isDone]{
                dispatchBlock();
                isDone.notify_one();
            });
            
            isDone.wait(lock);
        }
    }
}