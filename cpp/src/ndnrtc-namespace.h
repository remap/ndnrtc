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
        static const std::string NdnRtcNamespaceComponentApp;
        static const std::string NdnRtcNamespaceComponentUser;
        static const std::string NdnRtcNamespaceComponentBroadcast;
        static const std::string NdnRtcNamespaceComponentDiscovery;
        static const std::string NdnRtcNamespaceComponentUserStreams;
        static const std::string NdnRtcNamespaceComponentStreamAccess;        
        static const std::string NdnRtcNamespaceComponentStreamKey;
        static const std::string NdnRtcNamespaceComponentStreamFrames;
        static const std::string NdnRtcNamespaceComponentStreamInfo;
        static const std::string NdnRtcNamespaceKeyComponent;
        static const std::string NdnRtcNamespaceCertificateComponent;
        
        // composing URI based on provided components
        static shared_ptr<std::string> getProducerPrefix(const std::string &hub, const std::string &producerId);
        static shared_ptr<std::string> getStreamPath(const std::string &hub, const std::string &producerId, const std::string streamName);
        static shared_ptr<std::string> buildPath(bool precede, const std::string *component1, ...);
        static shared_ptr<const std::vector<unsigned char>> getFrameNumberComponent(long long frameNo);
        
        static shared_ptr<Name> keyPrefixForUser(const std::string &userPrefix);
        static shared_ptr<Name> certificateNameForUser(const std::string &userPrefix);
        static shared_ptr<KeyChain> keyChainForUser(const std::string &userPrefix);
        
    private:
    };
}

#endif /* defined(__ndnrtc__ndn_namespace__) */
