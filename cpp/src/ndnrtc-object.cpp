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

using namespace ndnrtc;

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnParams::Parameter::Parameter(ParameterType type, void *value): type_(type)
{
    int valueSize = 0;
    
    switch (type_) {
        case ParameterTypeBool:
            valueSize = sizeof(bool);
            break;
        case ParameterTypeInt:
            valueSize = sizeof(int);
            break;
        case ParameterTypeString:
            valueSize = strlen((const char*)value)+1;
            break;
    }
    
    value_ = malloc(valueSize);
    memset(value_,0,valueSize);
    memcpy(value_,value,valueSize);
}

//********************************************************************************
#pragma mark - public
void NdnParams::addParams(ndnrtc::NdnParams &params)
{
    std::map<std::string, Parameter>::iterator it;
    
    for (it = params.map().begin(); it != params.map().end(); ++it)
    {
        std::string name = it->first;
        Parameter p = it->second;
        
        setParam(name, Parameter(p.type_, p.value_));
    }
}
void NdnParams::resetParams(NdnParams &params)
{
    propertiesMap_.clear();
    addParams(params);
}
//********************************************************************************
#pragma mark - protected
int NdnParams::getParamAsInt(const std::string &paramName, int *param)
{
    Parameter p = propertiesMap_[paramName];
    
    if (p.type_ == NdnParams::ParameterTypeInt)
    {
        *param = *(int*)p.value_;
        return 0;
    }
    
    return -1;
}
int NdnParams::getParamAsBool(const std::string &paramName, bool *param)
{
    Parameter p = propertiesMap_[paramName];
    
    if (p.type_ == NdnParams::ParameterTypeBool)
    {
        *param = *(bool*)p.value_;
        return 0;
    }
    
    return -1;
}
int NdnParams::getParamAsString(const std::string &paramName, char **param)
{
    Parameter p = propertiesMap_[paramName];
    
    if (p.type_ == NdnParams::ParameterTypeString)
    {
        *param = (char*)p.value_;
        return 0;
    }
    
    return -1;    
}

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnRtcObject::NdnRtcObject() : observer_(NULL), params_(NULL)
{
}
NdnRtcObject::NdnRtcObject(NdnParams *params) : NdnRtcObject()
{
    params_ = new NdnParams(*params);
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
#pragma mark - public

//********************************************************************************
#pragma mark - intefaces realization - INdnRtcObjectObserver
void NdnRtcObject::onErrorOccurred(const char *errorMessage)
{
    ERROR("error occurred: %s", errorMessage);
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
        observer_->onErrorOccurred(emsg);
    else
        ERROR("%s", emsg);
    
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
