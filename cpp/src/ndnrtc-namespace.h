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
    using namespace ndn;
    
    class NdnRtcNamespace
    {
    public:
        // namespace components
        static const std::string NameComponentApp;
        static const std::string NameComponentUser;
        static const std::string NameComponentBroadcast;
        static const std::string NameComponentDiscovery;
        static const std::string NameComponentUserStreams;
        static const std::string NameComponentSession;
        static const std::string NameComponentStreamAccess;        
        static const std::string NameComponentStreamKey;
        static const std::string NameComponentStreamFrames;
        static const std::string NameComponentStreamFramesDelta;
        static const std::string NameComponentStreamFramesKey;
        static const std::string NameComponentStreamInfo;
        static const std::string NameComponentFrameSegmentData;
        static const std::string NameComponentFrameSegmentParity;
        static const std::string KeyComponent;
        static const std::string CertificateComponent;
        
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
        
        /**
         * Builds user stream prefix for current parameters
         * @param params Library parameters
         * @return User prefix (i.e. "<params.ndnHub>/<NameComponentApp>/<NameComponentUser>/<params.producerId>") 
         * or pointer to null in case of error
         */
        static boost::shared_ptr<std::string>
        getUserPrefix(const ParamsStruct &params);
        
        /**
         * Builds stream prefix for current parameters
         * @param params Library parameters
         * @return User's stream prefix in a form of "<user_prefix>/<NameComponentUserStreams>/<params.streamName>"
         * or pointer to null if an error
         * @see getUserPrefix for more info on form of "user_prefix" component
         */
        static boost::shared_ptr<std::string>
        getStreamPrefix(const ParamsStruct &params);
        
        /**
         * Builds stream frame prefix for current parameters
         * @param params Library parameters
         * @param isKeyNamespace indicates for which key types prefix should be 
         * built
         * @return User's stream frame prefix in a form of "<stream_prefix>/<params.streamThread>/<NameComponentStreamFrames>/<frame_type>"
         * where "frame_type" is either NameComponentStreamFramesDelta or 
         * NameComponentStreamFramesKey depending on isKeyNamespace paramter 
         * value or pointer to null if an error
         * @see getStreamPrefix for more info on form of "stream_prefix" component
         */
        static boost::shared_ptr<std::string>
        getStreamFramePrefix(const ParamsStruct &params,
                             int streamIdx,
                             bool isKeyNamespace = false);
        
        /**
         * Builds stream key prefix for current paramteres
         * @param params Library parameters
         * @return Stream's key prefix in a form of "<stream_prefix>/NameComponentStreamKey" 
         * or pointer to null if error occured
         * @see getStreamPrefix for more info on form ot "stream_prefix" component
         */
        static boost::shared_ptr<std::string>
        getStreamKeyPrefix(const ParamsStruct &params);
        
        /**
         * Builds session info prefix for current parameters
         * @param params Library parameters
         * @return User's session info prefix in a form of "<user_prefix>/<NameComponentUserStreams>/<NameComponentSession>
         * @see getUserPrefix for more info on form ot "user_prefix" component
         
         */
        static boost::shared_ptr<std::string>
        getSessionInfoPrefix(const ParamsStruct &params);
        
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
        static bool isDeltaFramesPrefix(const Name &prefix);
        static bool isParitySegmentPrefix(const Name &prefix);
        
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
        
        static int getStreamIdFromPrefix(const Name& prefix,
                                         const ParamsStruct& params);
        
    private:
    };
}

#endif /* defined(__ndnrtc__ndn_namespace__) */
