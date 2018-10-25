// 
// key-chain-manager.hpp
//
// Copyright (c) 2018. Regents of the University of California
// For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __keychain_manager_hpp__
#define __keychain_manager_hpp__

#include "../simple-log.hpp"

namespace ndn {
    class Face;
    class KeyChain;
    class PolicyManager;
    class Data;
    class Name;
    class IdentityStorage;
    class PrivateKeyStorage;
    class MemoryContentCache;
}

namespace ndnrtc {
    namespace helpers {

        class KeyChainManager : public ndnlog::new_api::ILoggingObject {
        public:
            KeyChainManager(boost::shared_ptr<ndn::Face> face,
                boost::shared_ptr<ndn::KeyChain> keyChain,
                const std::string& identityName,
                const std::string& instanceName,
                const std::string& configPolicy,
                unsigned int instanceCertLifetime,
                boost::shared_ptr<ndnlog::new_api::Logger> logger);
            ~KeyChainManager() {}

            boost::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
            boost::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }
            std::string instancePrefix() const { return instanceIdentity_; }

            const boost::shared_ptr<ndn::Data> instanceCertificate() const
            { return instanceCert_; }

            const boost::shared_ptr<ndn::Data> signingIdentityCertificate() const
            { return signingCert_; }

            // Will register prefixes for serving instance and signing certificates.
            // Maintains memory content cache for storing certificates.
            // Does not re-publbish certificates.
            void publishCertificates();

            boost::shared_ptr<ndn::MemoryContentCache> memoryContentCache() const
            { return memoryContentCache_; }

            // Creates a KeyChain. Depending on provided argument, either system default 
            // key chain will be created or file-based keychain will be created.
            // If storagePath != "" then file-base key chain is created. In this case,
            // under storagePath, two items will be created:
            //      - folder "keys" - for storing private and public keys
            //      - file "public-info.db" - for storing pubblic certificates
            // If these files already exist, will create key chain using them.
            static boost::shared_ptr<ndn::KeyChain> createKeyChain(std::string storagePath);

        private:
            boost::shared_ptr<ndn::Face> face_;
            std::string signingIdentity_, instanceName_,
            configPolicy_, instanceIdentity_;
            unsigned int runTime_;

            boost::shared_ptr<ndn::PolicyManager> configPolicyManager_;
            boost::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;
            boost::shared_ptr<ndn::Data> instanceCert_, signingCert_;
            boost::shared_ptr<ndn::IdentityStorage> identityStorage_;
            boost::shared_ptr<ndn::PrivateKeyStorage> privateKeyStorage_;
            boost::shared_ptr<ndn::MemoryContentCache> memoryContentCache_;

            void setupDefaultKeyChain();
            void setupInstanceKeyChain();
            void setupConfigPolicyManager();
            void createSigningIdentity();
            void createMemoryKeychain();
            void createInstanceIdentity();
            void createInstanceIdentityV2();
            void registerPrefix(const ndn::Name& prefix);
            void checkExists(const std::string&);
        };
    }
}

#endif