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
#include "name-components.h"

namespace ndnrtc {
    using namespace ndn;
    
    class NdnRtcNamespace
    {
    public:
        // composing URI based on provided components
        static boost::shared_ptr<std::string>
            getProducerPrefix(const std::string &hub,
                              const std::string &producerId);
        
        static boost::shared_ptr<std::string>
            getStreamPath(const std::string &hub,
                          const std::string &producerId,
                          const std::string streamName);
        
        static boost::shared_ptr<std::string>
            buildPath(bool precede, const std::string *component1, ...);
        
        static boost::shared_ptr<const std::vector<unsigned char> >
            getNumberComponent(long unsigned int frameNo);
        
        static boost::shared_ptr<std::string>
        getStreamPrefix(const std::string &userPrefix,
                        const std::string &streamName);
        
        static std::string
        getThreadPrefix(const std::string& streamPrefix,
                        const std::string& threadName);

        static std::string
        getThreadFramesPrefix(const std::string& threadPrefix,
                              bool isKeyNamespace = false);
        
        static boost::shared_ptr<std::string>
        getSessionInfoPrefix(const std::string& userPrefix);
        
        static boost::shared_ptr<Name> keyPrefixForUser(const std::string &userPrefix);
        static boost::shared_ptr<Name> certificateNameForUser(const std::string &userPrefix);
        
        /**
         * Returns KeyChain class configured to use MemoryIdentityStorage, 
         * MemoryPrivateKeyStorage, user-specific key name (@see keyPrefixForUser 
         * call) and pre-defined private and public keys (see DEFAULT_PUBLIC_KEY_DER 
         * and DEFAULT_PRIVATE_KEY_DER)
         */
        static boost::shared_ptr<KeyChain> keyChainForUser(const std::string &userPrefix);

        /**
         * Returns default KeyChain class
         */
        static boost::shared_ptr<KeyChain> defaultKeyChain();
        
        static void appendStringComponent(Name& prefix,
                                          const std::string& stringComponent);
        static void appendStringComponent(boost::shared_ptr<Name>& prefix,
                                          const std::string& stringComponent);
        
        static void appendDataKind(Name& prefix, bool shouldAppendParity);
        static void appendDataKind(boost::shared_ptr<Name>& prefix, bool shouldAppendParity);
        
        static bool hasComponent(const Name &prefix,
                                 const std::string &component,
                                 bool isClosed = true);
        static int findComponent(const Name &prefix,
                                   const std::string &component);
        
        static bool isValidInterestPrefix(const Name& prefix);
        static bool isValidPacketDataPrefix(const Name& prefix);
        
        static bool isKeyFramePrefix(const Name &prefix);
        static bool isDeltaFramePrefix(const Name &prefix);
        static bool isParitySegmentPrefix(const Name &prefix);
        static bool isPrefix(const Name &name, const Name &prefix);
        
        static PacketNumber getPacketNumber(const Name &prefix);
        static SegmentNumber getSegmentNumber(const Name &prefix);
        static void getSegmentationNumbers(const ndn::Name &prefix,
                                           PacketNumber& packetNumber,
                                           SegmentNumber& segmentNumber);
        
        static int trimSegmentNumber(const Name &prefix, Name &trimmedPrefix);
        static int trimPacketNumber(const Name &prefix, Name &trimmedPrefix);
        static int trimDataTypeComponent(const Name &prefix,
                                         Name &trimmedPrefix);
        
        static bool trimmedLookupPrefix(const Name& prefix, Name& lookupPrefix);
        
        static std::string getUserName(const std::string& prefix);
    private:
    };
}

#endif /* defined(__ndnrtc__ndn_namespace__) */
