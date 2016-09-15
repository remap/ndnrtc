// 
// key-chain-manager.cpp
//
//  Created by Peter Gusev on 02 September 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "key-chain-manager.h"
#include <algorithm>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndnrtc/simple-log.h>
#include <boost/chrono.hpp>

using namespace boost::chrono;

using namespace ndn;

KeyChainManager::KeyChainManager(const std::string& identityNameStr, 
	unsigned int runTime):signingIdentity_(identityNameStr), runTime_(runTime)
{
	setupDefaultKeyChain();
	setupInstanceKeyChain();
}

void KeyChainManager::setupDefaultKeyChain()
{
	defaultKeyChain_ = boost::make_shared<ndn::KeyChain>();
}

void KeyChainManager::setupInstanceKeyChain()
{
	Name signingIdentity(signingIdentity_);
	std::vector<Name> identities;

	defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, false);
	defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, true);

	if (std::find(identities.begin(), identities.end(), signingIdentity) == identities.end())
	{
		// create signing identity in default keychain
		LogInfo("") << "Signing identity not found. Creating..." << std::endl;
		createSigningIdentity();
	}

	createMemoryKeychain();
	createInstanceIdentity();
}

void KeyChainManager::createSigningIdentity()
{
    // create self-signed certificate
	Name cert = defaultKeyChain_->createIdentityAndCertificate(Name(signingIdentity_));

	LogWarn("") << "Generated identity " << signingIdentity_ << " (certificate name " 
		<< cert << ")" << std::endl;
	LogWarn("") << "Check policy config file for correct trust anchor (run `ndnsec-dump-certificate -i " 
		<< signingIdentity_  << " > headless.cert` if needed)" << std::endl;
}

void KeyChainManager::createMemoryKeychain()
{
    boost::shared_ptr<MemoryIdentityStorage> identityStorage = boost::make_shared<MemoryIdentityStorage>();
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage = boost::make_shared<MemoryPrivateKeyStorage>();
    instanceKeyChain_ = boost::make_shared<KeyChain>
      (boost::make_shared<IdentityManager>(identityStorage, privateKeyStorage),
       boost::make_shared<ConfigPolicyManager>("rule.conf"));
}

void KeyChainManager::createInstanceIdentity()
{
	auto now = system_clock::now().time_since_epoch();
	duration<double> sec = now;
	std::stringstream ss;
	ss << std::fixed << sec.count();

	Name instanceIdentity(signingIdentity_);
	instanceIdentity.append(ss.str());

	LogInfo("") << "Instance identity " << instanceIdentity << std::endl;

	Name instanceKeyName = instanceKeyChain_->generateRSAKeyPairAsDefault(instanceIdentity);
	Name signingCert = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(signingIdentity_));

	LogDebug("") << "Instance key " << instanceKeyName << std::endl;
	LogDebug("") << "Signing certificate " << signingCert << std::endl;

	std::vector<CertificateSubjectDescription> subjectDescriptions;
	boost::shared_ptr<IdentityCertificate> instanceCert = 
		instanceKeyChain_->getIdentityManager()->prepareUnsignedIdentityCertificate(instanceKeyName,
			Name(signingIdentity_),
			sec.count()*1000,
			(sec+duration<double>(runTime_)).count()*1000,
			subjectDescriptions);
	defaultKeyChain_->sign(*instanceCert, signingCert);
	instanceKeyChain_->installIdentityCertificate(*instanceCert);
	instanceKeyChain_->setDefaultCertificateForKey(*instanceCert);

	LogDebug("") << "Instance certificate " 
		<< instanceKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(instanceIdentity)) << std::endl;
}
