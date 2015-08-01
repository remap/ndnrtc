//
//  ndnrtc-object.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

#include <boost/thread/lock_guard.hpp>
#include <boost/chrono.hpp>

#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace std;
using namespace ndnlog;
using namespace boost;

//******************************************************************************
/**
 * @name NdnRtcComponent class
 */
NdnRtcComponent::NdnRtcComponent():
watchdogTimer_(NdnRtcUtils::getIoService()),
isJobScheduled_(false)
{}

NdnRtcComponent::NdnRtcComponent(INdnRtcComponentCallback *callback):
callback_(callback),
watchdogTimer_(NdnRtcUtils::getIoService()),
isJobScheduled_(false)
{}

NdnRtcComponent::~NdnRtcComponent()
{
    stopJob();
}

void NdnRtcComponent::onError(const char *errorMessage, const int errorCode)
{
    if (hasCallback())
    {
        lock_guard<mutex> scopedLock(callbackMutex_);
        callback_->onError(errorMessage, errorCode);
    }
    else
    {
        LogErrorC << "error occurred: " << string(errorMessage) << endl;
        if (logger_) logger_->flush();
    }
}

int NdnRtcComponent::notifyError(const int ecode, const char *format, ...)
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    if (hasCallback())
    {
        callback_->onError(emsg, ecode);
    }
    else
        LogErrorC
        << "error (" << ecode << ") occurred: "
        << string(emsg) << endl;
    
    return ecode;
}

std::string
NdnRtcComponent::getDescription() const
{
    if (description_ == "")
    {
        std::stringstream ss;
        ss << "NdnRtcObject "<< std::hex << this;
        return ss.str();
    }
    
    return description_;
}

thread
NdnRtcComponent::startThread(function<bool ()> threadFunc)
{
    thread threadObject = thread([threadFunc](){
        bool result = false;
        do {
            try
            {
                this_thread::interruption_point();
                {
                    this_thread::disable_interruption di;
                    result = threadFunc();
                }
            }
            catch (thread_interrupted &interruption)
            {
                result = false;
            }
        } while (result);
    });
    
    return threadObject;
}

void
NdnRtcComponent::stopThread(thread &threadObject)
{
    threadObject.interrupt();
    
    if (threadObject.joinable())
    {
        bool res = threadObject.try_join_for(chrono::milliseconds(500));
                                             
        if (!res)
            threadObject.detach();
    }
}

void NdnRtcComponent::scheduleJob(const unsigned int usecInterval,
                                  boost::function<bool()> jobCallback)
{
    if (!thisShared_)
        thisShared_ = shared_from_this();
    
    boost::lock_guard<boost::recursive_mutex> scopedLock(this->timerMutex_);
    
    watchdogTimer_.expires_from_now(boost::chrono::microseconds(usecInterval));
    isJobScheduled_ = true;
    
    watchdogTimer_.async_wait([this, usecInterval, jobCallback](const boost::system::error_code& code){
        boost::lock_guard<boost::recursive_mutex> scopedLock(this->timerMutex_);
        isJobScheduled_ = false;
        
        if (code == boost::asio::error::operation_aborted)
        {
            timerDone_.notify_one();
        }
        else
        {
            bool res = jobCallback();
            if (res)
                this->scheduleJob(usecInterval, jobCallback);
        }
    });

}

void NdnRtcComponent::stopJob()
{
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(timerMutex_);
        watchdogTimer_.cancel();
    }
    
    if (isJobScheduled_ && !NdnRtcUtils::isBackgroundThread())
    {
        boost::mutex m;
        boost::unique_lock<boost::mutex> lock(m);
        timerDone_.wait(lock);
    }
}
