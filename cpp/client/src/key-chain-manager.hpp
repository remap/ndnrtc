// 
// key-chain-manager.h
//
//  Created by Peter Gusev on 02 September 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __key_chain_manager_h__
#define __key_chain_manager_h__

#include <boost/shared_ptr.hpp>

namespace ndn {
	class KeyChain;
    class Face;
    class MemoryContentCache;
    class IdentityCertificate;
}

class KeyChainManager {
public:
    KeyChainManager(boost::shared_ptr<ndn::Face> face,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int runTime);

	boost::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
	boost::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }
    std::string instancePrefix() const { return instanceIdentity_; }
    
    const boost::shared_ptr<ndn::IdentityCertificate> instanceCertificate() const
        { return instanceCert_; }
    
private:
    boost::shared_ptr<ndn::Face> face_;
	std::string signingIdentity_, instanceName_,
        configPolicy_, instanceIdentity_;
	unsigned int runTime_;

	boost::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;
    boost::shared_ptr<ndn::IdentityCertificate> instanceCert_;

	void setupDefaultKeyChain();
	void setupInstanceKeyChain();
	void createSigningIdentity();
	void createMemoryKeychain();
	void createInstanceIdentity();
    void checkExists(const std::string&);
};

#endif
