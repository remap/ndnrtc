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
    class IdentityStorage;
    class PrivateKeyStorage;
}

class KeyChainManager : public ndnlog::new_api::ILoggingObject {
public:
    KeyChainManager(boost::shared_ptr<ndn::Face> face,
                    boost::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    boost::shared_ptr<ndnlog::new_api::Logger> logger);

    boost::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
    boost::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }
    std::string instancePrefix() const { return instanceIdentity_; }
    
    const boost::shared_ptr<ndn::Data> instanceCertificate() const
    { return instanceCert_; }

    const boost::shared_ptr<ndn::Data> signingIdentityCertificate() const
    { return signingCert_; }
    
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

    void setupDefaultKeyChain();
    void setupInstanceKeyChain();
    void setupConfigPolicyManager();
    void createSigningIdentity();
    void createMemoryKeychain();
    void createInstanceIdentity();
    void createInstanceIdentityV2();
    void checkExists(const std::string&);
};

#endif