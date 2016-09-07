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
}

class KeyChainManager {
public:
	KeyChainManager(const std::string& identityName, 
		unsigned int runTime);

	boost::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
	boost::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }

private:
	std::string signingIdentity_;
	unsigned int runTime_;

	boost::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;

	void setupDefaultKeyChain();
	void setupInstanceKeyChain();
	void createSigningIdentity();
	void createMemoryKeychain();
	void createInstanceIdentity();
};

#endif
