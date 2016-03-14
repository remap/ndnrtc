//
//  name-components.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "name-components.h"
#include "ndnrtc-namespace.h"

using namespace ndnrtc;

const std::string NameComponents::NameComponentApp = "ndnrtc";
const std::string NameComponents::NameComponentUser = "user";
const std::string NameComponents::NameComponentSession = "session-info";
const std::string NameComponents::NameComponentBroadcast = "/ndn/broadcast";
const std::string NameComponents::NameComponentDiscovery = "discovery";
const std::string NameComponents::NameComponentUserStreams = "streams";
const std::string NameComponents::NameComponentStreamAccess = "access";
const std::string NameComponents::NameComponentStreamKey = "key";
const std::string NameComponents::NameComponentStreamFramesDelta = "delta";
const std::string NameComponents::NameComponentStreamFramesKey = "key";
const std::string NameComponents::NameComponentStreamInfo = "info";
const std::string NameComponents::NameComponentFrameSegmentData = "data";
const std::string NameComponents::NameComponentFrameSegmentParity = "parity";
const std::string NameComponents::KeyComponent = "DSK-1408";
const std::string NameComponents::CertificateComponent = "KEY/ID-CERT/0";

std::string
NameComponents::getUserPrefix(const std::string& username,
                              const std::string& prefix)
{
    return *NdnRtcNamespace::getProducerPrefix(prefix, username);
}

std::string
NameComponents::getStreamPrefix(const std::string& streamName,
                                const std::string& username,
                                const std::string& prefix)
{
    return *NdnRtcNamespace::getStreamPath(prefix, username, streamName);
}

std::string
NameComponents::getThreadPrefix(const std::string& threadName,
                                const std::string& streamName,
                                const std::string& username,
                                const std::string& prefix)
{
    return NdnRtcNamespace::getThreadPrefix(*NdnRtcNamespace::getStreamPath(prefix, username, streamName), threadName);
}

std::string 
NameComponents::getUserName(const std::string& prefix)
{
    size_t userComp = prefix.find(NameComponents::NameComponentUser);
    
    if (userComp != std::string::npos)
    {
        size_t startPos = userComp+NameComponents::NameComponentUser.size()+1;
        if (prefix.size() >= startPos)
        {
            size_t endPos = prefix.find("/", startPos);

            if (endPos == std::string::npos)
                endPos = prefix.size();
            return prefix.substr(startPos, endPos-startPos);
        }
    }
        
    return "";
}

std::string 
NameComponents::getStreamName(const std::string& prefix)
{
	std::string userName = NameComponents::getUserName(prefix);

	if (userName == "") return "";

    size_t p = prefix.find(userName);
    size_t startPos = p+userName.size()+1;
    
    if (prefix.size() >= startPos)
    {
        size_t endPos = prefix.find("/", startPos);

        if (endPos == std::string::npos)
            endPos = prefix.size();
        return prefix.substr(startPos, endPos-startPos);
    }
        
    return "";
}

std::string 
NameComponents::getThreadName(const std::string& prefix)
{
	std::string streamName = NameComponents::getStreamName(prefix);

	if (streamName == "") return "";

    size_t p = prefix.find(streamName);
    size_t startPos = p+streamName.size()+1;

    if (prefix.size() >= startPos)
    {
        size_t endPos = prefix.find("/", startPos);

        if (endPos == std::string::npos)
            endPos = prefix.size();
        return prefix.substr(startPos, endPos-startPos);
    }
        
    return "";
}