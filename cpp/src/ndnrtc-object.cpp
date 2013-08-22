//
//  ndnrtc-object.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "ndnrtc-object.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnRtcObject::NdnRtcObject() : observer_(NULL), params_(NULL)
{
}
NdnRtcObject::NdnRtcObject(NdnParams *params) : NdnRtcObject()
{
    params_ = new NdnParams(params);
}
NdnRtcObject::NdnRtcObject(NdnParams *params, INdnRtcObjectObserver *observer) : NdnRtcObject(params)
{
    observer_ = observer;
}
NdnRtcObject::~NdnRtcObject()
{
    delete params_;
}

//********************************************************************************
#pragma mark - intefaces realization - INdnRtcObjectObserver
void NdnRtcObject::onErrorOccurred(const char *errorMessage)
{
    ERROR("error occurred: %s", errorMessage);
}

//********************************************************************************
#pragma mark - protected
int NdnRtcObject::notifyError(const int ecode, const char *emsg)
{
    if (hasObserver())
        observer_->onErrorOccurred(emsg);
    else
        ERROR("%s", emsg);
    
    return ecode;
}

int NdnRtcObject::notifyNoParams()
{
    return notifyError(-1, "no parameters provided");
}
