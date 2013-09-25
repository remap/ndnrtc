//
//  ndnrtc-namespace.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "ndnrtc-namespace.h"

using namespace std;
using namespace ndnrtc;

//********************************************************************************
#pragma mark - all static

const std::string NdnRtcNamespace::NdnRtcNamespaceComponentApp = "ndnrtc";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentUser = "user";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentBroadcast = "/ndn/broadcast";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentDiscovery = "discovery";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentUserStreams = "streams";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentStreamAccess = "access";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentStreamKey = "key";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentStreamFrames = "frames";
const std::string NdnRtcNamespace::NdnRtcNamespaceComponentStreamInfo = "info";

shared_ptr<std::string> NdnRtcNamespace::getProducerPrefix(const std::string &hub, const std::string &producerId)
{
    return buildPath(true, &hub, &NdnRtcNamespaceComponentApp, &NdnRtcNamespaceComponentUser, &producerId, NULL);
}

shared_ptr<std::string> NdnRtcNamespace::getStreamPath(const std::string &hub, const std::string &producerId, const std::string streamName)
{
    shared_ptr<std::string> producerPrefix = getProducerPrefix(hub, producerId);
    
    return buildPath(false, producerPrefix.get(), &NdnRtcNamespaceComponentUserStreams, &streamName, NULL);
}

shared_ptr<std::string> NdnRtcNamespace::buildPath(bool precede, const std::string *component1, ...)
{
    shared_ptr<std::string> path(new std::string(""));
    va_list ap;
    va_start(ap, component1);
    const std::string *s = component1;
    std::string delim = "/";
    
    while (s)
    {
        if (precede)
        {
            precede = false;
            path.get()->append(delim);
        }
        
        path.get()->append(*s);
        
        s = va_arg(ap, const std::string*);
        
        if (s)
            path.get()->append(delim);
    }
    
    va_end(ap);
    
    return path;
}

//#define MLC

shared_ptr<const std::vector<unsigned char>> NdnRtcNamespace::getFrameNumberComponent(long long frameNo)
{
    char value[10];
    
    memset(&value[0],0,10);
    sprintf(value, "%lld", frameNo);
    
    unsigned int valueLength = strlen(value);
    shared_ptr<vector<unsigned char>> component(new vector<unsigned char>(value, value + valueLength));
    
    return component;
}

//********************************************************************************
#pragma mark - private