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

#include <boost/chrono.hpp>

#include "ndnrtc-object.h"

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
NdnRtcComponent::NdnRtcComponent()
{}

NdnRtcComponent::NdnRtcComponent(INdnRtcComponentCallback *callback):
callback_(callback)
{}

NdnRtcComponent::~NdnRtcComponent()
{
    std::cout << description_ << " component dtor" << std::endl;
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
