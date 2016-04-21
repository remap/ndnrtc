//
//  name-components.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef libndnrtc_ndnrtc_name_components_h
#define libndnrtc_ndnrtc_name_components_h

#include <string>
#include <ndn-cpp/name.hpp>

#include "params.h"

namespace ndnrtc {
    class NameComponents {
    public:
        #if 0
        static const std::string NameComponentApp;
        static const std::string NameComponentUser;
        static const std::string NameComponentBroadcast;
        static const std::string NameComponentDiscovery;
        static const std::string NameComponentUserStreams;
        static const std::string NameComponentSession;
        static const std::string NameComponentStreamAccess;
        static const std::string NameComponentStreamKey;
        static const std::string NameComponentStreamFramesDelta;
        static const std::string NameComponentStreamFramesKey;
        static const std::string NameComponentStreamInfo;
        static const std::string NameComponentFrameSegmentData;
        static const std::string NameComponentFrameSegmentParity;
        static const std::string KeyComponent;
        static const std::string CertificateComponent;
        
        static std::string
        getUserPrefix(const std::string& username,
                      const std::string& prefix);
        
        static std::string
        getStreamPrefix(const std::string& streamName,
                        const std::string& username,
                        const std::string& prefix);
        
        static std::string
        getThreadPrefix(const std::string& threadName,
                        const std::string& streamName,
                        const std::string& username,
                        const std::string& prefix);

        static std::string 
        getUserName(const std::string& prefix);

        static std::string 
        getStreamName(const std::string& prefix);

        static std::string 
        getThreadName(const std::string& prefix);
        #endif

        //****************************************************************************
        static const std::string NameComponentApp;
        static const std::string NameComponentAudio;
        static const std::string NameComponentVideo;
        static const std::string NameComponentMeta;
        static const std::string NameComponentDelta;
        static const std::string NameComponentKey;
        static const std::string NameComponentParity;

        static unsigned int 
        nameApiVersion();

        static ndn::Name
        ndnrtcSuffix();

        static ndn::Name
        streamPrefix(MediaStreamParams::MediaStreamType type, std::string basePrefix);

        static ndn::Name
        audioStreamPrefix(std::string basePrefix);

        static ndn::Name
        videoStreamPrefix(std::string basePrefix);
    };
}

#endif
