//
//  ndnrtc-namespace.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 9/17/13
//

#ifndef __ndnrtc__ndn_namespace__
#define __ndnrtc__ndn_namespace__

#include "ndnrtc-object.h"

namespace ndnrtc {
    class NdnRtcNamespace
    {
    public:
        // namespace components
        static const std::string NameComponentApp;
        static const std::string NameComponentUser;
        static const std::string NameComponentBroadcast;
        static const std::string NameComponentDiscovery;
        static const std::string NameComponentUserStreams;
        static const std::string NameComponentStreamAccess;        
        static const std::string NameComponentStreamKey;
        static const std::string NameComponentStreamFrames;
        static const std::string NameComponentStreamFramesDelta;
        static const std::string NameComponentStreamFramesKey;
        static const std::string NameComponentStreamInfo;
        static const std::string KeyComponent;
        static const std::string CertificateComponent;
        
        // composing URI based on provided components
        static shared_ptr<std::string>
            getProducerPrefix(const std::string &hub,
                              const std::string &producerId);
        
        static shared_ptr<std::string>
            getStreamPath(const std::string &hub,
                          const std::string &producerId,
                          const std::string streamName);
        
        static shared_ptr<std::string>
            buildPath(bool precede, const std::string *component1, ...);
        
        static shared_ptr<const std::vector<unsigned char>>
            getNumberComponent(long unsigned int frameNo);
        
        static shared_ptr<Name> keyPrefixForUser(const std::string &userPrefix);
        static shared_ptr<Name> certificateNameForUser(const std::string &userPrefix);
        static shared_ptr<KeyChain> keyChainForUser(const std::string &userPrefix);
        
    private:
    };
}

#endif /* defined(__ndnrtc__ndn_namespace__) */
