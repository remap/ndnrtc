//
//  name-components.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <string> 
#include <sstream>
#include <algorithm>
#include <iterator>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "name-components.h"
#include "ndnrtc-namespace.h"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

const string NameComponents::NameComponentApp = "ndnrtc";
#if 0
const string NameComponents::NameComponentUser = "user";
const string NameComponents::NameComponentSession = "session-info";
const string NameComponents::NameComponentBroadcast = "/ndn/broadcast";
const string NameComponents::NameComponentDiscovery = "discovery";
const string NameComponents::NameComponentUserStreams = "streams";
const string NameComponents::NameComponentStreamAccess = "access";
const string NameComponents::NameComponentStreamKey = "key";
const string NameComponents::NameComponentStreamFramesDelta = "delta";
const string NameComponents::NameComponentStreamFramesKey = "key";
const string NameComponents::NameComponentStreamInfo = "info";
const string NameComponents::NameComponentFrameSegmentData = "data";
const string NameComponents::NameComponentFrameSegmentParity = "parity";
const string NameComponents::KeyComponent = "DSK-1408";
const string NameComponents::CertificateComponent = "KEY/ID-CERT/0";
#endif

const string NameComponents::NameComponentAudio = "audio";
const string NameComponents::NameComponentVideo = "video";
const string NameComponents::NameComponentMeta = "_meta";
const string NameComponents::NameComponentDelta = "d";
const string NameComponents::NameComponentKey = "k";
const string NameComponents::NameComponentParity = "_parity";

#if 0
string
NameComponents::getUserPrefix(const string& username,
                              const string& prefix)
{
    return *NdnRtcNamespace::getProducerPrefix(prefix, username);
}

string
NameComponents::getStreamPrefix(const string& streamName,
                                const string& username,
                                const string& prefix)
{
    return *NdnRtcNamespace::getStreamPath(prefix, username, streamName);
}

string
NameComponents::getThreadPrefix(const string& threadName,
                                const string& streamName,
                                const string& username,
                                const string& prefix)
{
    return NdnRtcNamespace::getThreadPrefix(*NdnRtcNamespace::getStreamPath(prefix, username, streamName), threadName);
}

string 
NameComponents::getUserName(const string& prefix)
{
    size_t userComp = prefix.find(NameComponents::NameComponentUser);
    
    if (userComp != string::npos)
    {
        size_t startPos = userComp+NameComponents::NameComponentUser.size()+1;
        if (prefix.size() >= startPos)
        {
            size_t endPos = prefix.find("/", startPos);

            if (endPos == string::npos)
                endPos = prefix.size();
            return prefix.substr(startPos, endPos-startPos);
        }
    }
        
    return "";
}

string 
NameComponents::getStreamName(const string& prefix)
{
    size_t userComp = prefix.find(NameComponents::NameComponentUserStreams);
    
    if (userComp != string::npos)
    {
        size_t startPos = userComp+NameComponents::NameComponentUserStreams.size()+1;
        if (prefix.size() >= startPos)
        {
            size_t endPos = prefix.find("/", startPos);

            if (endPos == string::npos)
                endPos = prefix.size();
            return prefix.substr(startPos, endPos-startPos);
        }
    }

#if 0 // this code if for updated namespace
	string userName = NameComponents::getUserName(prefix);

	if (userName == "") return "";

    size_t p = prefix.find(userName);
    size_t startPos = p+userName.size()+1;
    
    if (prefix.size() >= startPos)
    {
        size_t endPos = prefix.find("/", startPos);

        if (endPos == string::npos)
            endPos = prefix.size();
        return prefix.substr(startPos, endPos-startPos);
    }
#endif
    return "";
}

string 
NameComponents::getThreadName(const string& prefix)
{
	string streamName = NameComponents::getStreamName(prefix);

	if (streamName == "") return "";

    size_t p = prefix.find(streamName);
    size_t startPos = p+streamName.size()+1;

    if (prefix.size() >= startPos)
    {
        size_t endPos = prefix.find("/", startPos);

        if (endPos == string::npos)
            endPos = prefix.size();
        return prefix.substr(startPos, endPos-startPos);
    }
        
    return "";
}
#endif

//******************************************************************************
vector<string> ndnrtcVersionComponents()
{
    string version(PACKAGE_VERSION);
    std::vector<string> components;
    
    boost::split(components, version, boost::is_any_of("."), boost::token_compress_on);
    
    return components;
}

unsigned int
NameComponents::nameApiVersion()
{
    return (unsigned int)atoi(ndnrtcVersionComponents().front().c_str());
}

Name
NameComponents::ndnrtcSuffix()
{
    return Name(NameComponentApp).appendVersion(nameApiVersion());
}

Name
NameComponents::audioStreamPrefix(string basePrefix)
{
    Name n = Name(basePrefix);
    return n.append(ndnrtcSuffix()).append(NameComponentAudio);
}

Name
NameComponents::videoStreamPrefix(string basePrefix)
{
    Name n = Name(basePrefix);
    return n.append(ndnrtcSuffix()).append(NameComponentVideo);
}
