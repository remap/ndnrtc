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
using namespace std;
using namespace ndnlog;

//********************************************************************************
/**
 * @name NdnRtcObject class
 */
#pragma mark - construction/destruction
NdnRtcObject::NdnRtcObject(const ParamsStruct &params, NdnLogger *logger):
LoggerObject(logger),
params_(params),
callbackSync_(*CriticalSectionWrapper::CreateCriticalSection())
{
  
}

NdnRtcObject::NdnRtcObject(const ParamsStruct &params,
                           NdnLogger *logger,
                           INdnRtcObjectObserver *observer):
NdnRtcObject(params, logger)
{
  observer_ = observer;
}

NdnRtcObject::~NdnRtcObject()
{
}

//********************************************************************************
#pragma mark - intefaces realization - INdnRtcObjectObserver
void NdnRtcObject::onErrorOccurred(const char *errorMessage)
{
  callbackSync_.Enter();
  if (hasObserver())
    observer_->onErrorOccurred(errorMessage);
  else
    NDNERROR("error occurred: %s", errorMessage);
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
    TRACE("error occurred: %s",emsg);
    observer_->onErrorOccurred(emsg);
  }
  else
    NDNERROR("%s", emsg);
  
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
