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
using namespace std;
//********************************************************************************
/**
 * @name Paramter class
 */
//********************************************************************************
#pragma mark - construction/destruction
NdnParams::Parameter::Parameter(const ParameterType type, const void *value): type_(type)
{
    value_ = NdnParams::Parameter::copiedValue(type_, value);
}
#pragma mark - public
void NdnParams::Parameter::setTypeAndValue(const ParameterType type, const void *value)
{
    type_ = type;
    
    if (value_ != nullptr)
        free(value_);
    
    value_ = NdnParams::Parameter::copiedValue(type_, value);
}
//********************************************************************************
#pragma mark - private
void* NdnParams::Parameter::copiedValue(const ParameterType type, const void *value)
{
    int valueSize = NdnParams::Parameter::valueByteSize(type, value);
    void *newValue = nullptr;
    
    if (valueSize)
    {
        newValue = malloc(valueSize);   
        memset(newValue, 0, valueSize);
        memcpy(newValue, value, valueSize);
    }
    
    return newValue;
}
int NdnParams::Parameter::valueByteSize(const ParameterType type, const void *value)
{
    int valueSize = 0;
    
    switch (type) {
        case ParameterTypeBool:
            valueSize = sizeof(bool);
            break;
        case ParameterTypeInt:
            valueSize = sizeof(int);
            break;
        case ParameterTypeString:
            valueSize = strlen((const char*)value)+1;
            break;
        case ParameterTypeUndefined: // fall through
        default:
            valueSize = 0;
            break;
    }
    return valueSize;
}
//********************************************************************************
/**
 * @name NdnParams class
 */
//********************************************************************************
//********************************************************************************
#pragma mark - all static
const string NdnParams::ParamNameDeviceId = "device_id";
const string NdnParams::ParamNameWidth = "capture_width";
const string NdnParams::ParamNameHeight = "capture_height";
const string NdnParams::ParamNameFPS = "fps";

const string NdnParams::ParamNameWindowWidth = "render_width";
const string NdnParams::ParamNameWindowHeight = "render_height";

const string NdnParams::ParamNameFrameRate = "frame_rate";
const string NdnParams::ParamNameStartBitRate = "start_bitrate";
const string NdnParams::ParamNameMaxBitRate = "max_bitrate";
const string NdnParams::ParamNameEncodeWidth = "encode_width";
const string NdnParams::ParamNameEncodeHeight = "encode_height";

const string NdnParams::ParamNameStreamPrefix = "stream_prefix";
const string NdnParams::ParamNameNdnHub = "ndn_hub";
const string NdnParams::ParamNameProducerId = "producer_id";
const string NdnParams::ParamNameStreamName = "stream_name";
const string NdnParams::ParamNameSegmentSize = "segment_size";
const string NdnParams::ParamNameFrameFreshnessInterval = "freshness";

const string NdnParams::ParamNameProducerRate = "playback_rate";
const string NdnParams::ParamNameReceiverId = "receiver_id";
const string NdnParams::ParamNameInterestTimeout = "interest_timeout";
const string NdnParams::ParamNameFrameBufferSize = "buffer_size";
const string NdnParams::ParamNameFrameSlotSize = "slot_size";
// parameters names
const string NdnParams::ParamNameConnectHost = "connect_host";
const string NdnParams::ParamNameConnectPort = "connect_port";

//********************************************************************************
#pragma mark - protected
void NdnParams::setParam(const std::string &name, const ndnrtc::NdnParams::Parameter &param)
{
    Parameter *newP = nullptr;
    
    newP = &propertiesMap_[name]; // returns existing or new
    newP->setTypeAndValue(param.getType(), param.getValue());
}
int NdnParams::getParamAsInt(const std::string &paramName, int *param) const
{
    Parameter *p = getParam(paramName);
    
    if (p && p->getType() == NdnParams::ParameterTypeInt)
    {
        *param = *(int*)p->getValue();
        return 0;
    }
    
    return -1;
}
int NdnParams::getParamAsBool(const std::string &paramName, bool *param) const
{
    Parameter *p = getParam(paramName);
    
    if (p && p->getType() == NdnParams::ParameterTypeBool)
    {
        *param = *(bool*)p->getValue();
        return 0;
    }
    
    return -1;
}
int NdnParams::getParamAsString(const std::string &paramName, char **param) const
{
    Parameter *p = getParam(paramName);
    
    if (p && p->getType() == NdnParams::ParameterTypeString)
    {
        *param = (char*)NdnParams::Parameter::copiedValue(p->getType(), p->getValue());
        return 0;
    }
    
    return -1;    
}

std::string NdnParams::getParamAsString(const std::string &paramName) const
{
    Parameter *p = getParam(paramName);
    char *param = NULL;
    
    if (p && p->getType() == NdnParams::ParameterTypeString)
    {
        param = (char*)NdnParams::Parameter::copiedValue(p->getType(), p->getValue());
    }
    
    std::string paramStr("");
    
    if (param)
    {
        paramStr = (const char*)param;
        free(param);
    }
    
    return paramStr;
}

int NdnParams::getParamAsInt(const std::string &paramName) const
{
    int param = -INT_MAX;
    
    getParamAsInt(paramName, &param);
    
    return param;
}

bool NdnParams::getParamAsBool(const std::string &paramName) const
{
    bool param = false;
    
    getParamAsBool(paramName, &param);
    
    return param;
}

//********************************************************************************
#pragma mark - private
NdnParams::Parameter* NdnParams::getParam(const std::string &name) const
{
    std::map<std::string, NdnParams::Parameter>::const_iterator it;
    
    it = propertiesMap_.find(name);
    
    if (it != propertiesMap_.end())
        return (NdnParams::Parameter*)&it->second;
    
    return nullptr;
}
//********************************************************************************
/**
 * @name NdnRtcObject class
 */
#pragma mark - construction/destruction
NdnRtcObject::NdnRtcObject() : observer_(nullptr), params_(nullptr)
{
}
NdnRtcObject::NdnRtcObject(const NdnParams *params) : observer_(nullptr)
{
    params_ = new NdnParams(*params);
}
NdnRtcObject::NdnRtcObject(const NdnParams *params, INdnRtcObjectObserver *observer) : NdnRtcObject(params)
{
    observer_ = observer;
}
NdnRtcObject::~NdnRtcObject()
{
    delete params_;
}

//********************************************************************************
#pragma mark - public
void NdnParams::addParams(const ndnrtc::NdnParams& params)
{
    const std::map<std::string, Parameter> &map = params.map();
    std::map<std::string, Parameter>::const_iterator it;
    
    for (it = map.begin(); it != map.end(); ++it)
    {
        std::string name = it->first;
        const Parameter *p = &it->second;
        
        setParam(name, Parameter(p->getType(), p->getValue()));
    }
}
void NdnParams::resetParams(const NdnParams &params)
{
    propertiesMap_.clear();
    addParams(params);
}
string NdnParams::description() const
{
    stringstream desc;
    
    std::map<std::string, NdnParams::Parameter>::const_iterator it;
    
    it = propertiesMap_.begin();
    
    while (it != propertiesMap_.end())
    {
        desc << it->first << ": ";
        
        const Parameter &p = it->second;
        
        switch (p.type_) {
            case ParameterTypeBool:
            {
                if (*((bool*)p.getValue()))
                    desc << "true";
                else
                    desc << "false";
            }
                break;
            case ParameterTypeInt:
            {
                desc << *((int*)p.getValue());
            }
                break;
            case ParameterTypeString:
            {
                desc << ((char*)p.getValue());
            }
                break;
            default:
                break;
        }
        
        desc << endl;
        it++;
    }
    
    return desc.str();
}

//********************************************************************************
#pragma mark - intefaces realization - INdnRtcObjectObserver
void NdnRtcObject::onErrorOccurred(const char *errorMessage)
{
    if (hasObserver())
        observer_->onErrorOccurred(errorMessage);
    else
        ERR("error occurred: %s", errorMessage);
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
        ERR("%s", emsg);
    
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
