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

#include "ndnrtc-object.h"

using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace std;
using namespace ndnlog;

//******************************************************************************
/**
 * @name NdnRtcComponent class
 */
NdnRtcComponent::NdnRtcComponent():
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{}

NdnRtcComponent::NdnRtcComponent(INdnRtcComponentCallback *callback):
callback_(callback),
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{}

NdnRtcComponent::~NdnRtcComponent()
{
    callbackSync_.~CriticalSectionWrapper();
}

void NdnRtcComponent::onError(const char *errorMessage)
{
    callbackSync_.Enter();
    
    if (hasCallback())
        callback_->onError(errorMessage);
    else
        LogErrorC << "error occurred: " << string(errorMessage) << endl;
    
    callbackSync_.Leave();
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
        callback_->onError(emsg);
    }
    else
        LogErrorC << "error occurred: " << string(emsg) << endl;
    
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

//********************************************************************************
/**
 * @name NdnRtcObject class
 */
#pragma mark - construction/destruction
NdnRtcObject::NdnRtcObject(const ParamsStruct &params):
params_(params),
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{
  
}

NdnRtcObject::NdnRtcObject(const ParamsStruct &params,
                           INdnRtcObjectObserver *observer):
params_(params),
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{
  observer_ = observer;
}

NdnRtcObject::~NdnRtcObject()
{
    callbackSync_.~CriticalSectionWrapper();
}

//********************************************************************************
#pragma mark - intefaces realization - INdnRtcObjectObserver
void NdnRtcObject::onErrorOccurred(const char *errorMessage)
{
  callbackSync_.Enter();
    
  if (hasObserver())
    observer_->onErrorOccurred(errorMessage);
  else
    LogErrorC << "error occurred: " << string(errorMessage) << endl;
    
  callbackSync_.Leave();
}

//********************************************************************************
#pragma mark - protected
int NdnRtcObject::notifyError(const int ecode, const char *format, ...)
{
  va_list args;
  
  static char emsg[256];
  
  va_start(args, format);
  vsprintf(emsg, format, args);
  va_end(args);
  
  if (hasObserver())
  {
    observer_->onErrorOccurred(emsg);
  }
  else
      LogErrorC << "error occurred: " << string(emsg) << endl;
  
  return ecode;
}

int NdnRtcObject::notifyErrorNoParams()
{
  return notifyError(-1, "no parameters provided");
}

int NdnRtcObject::notifyErrorBadArg(const std::string &paramName)
{
  return notifyError(-1, "bad or non-existent argument: %s", paramName.c_str());
}

std::string
NdnRtcObject::getDescription() const
{
    if (description_ == "")
    {
        std::stringstream ss;
        ss << "NdnRtcObject "<< std::hex << this;
        return ss.str();
    }
    
    return description_;
}
