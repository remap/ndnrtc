// 
// key-chain-manager.cpp
//
//  Created by Peter Gusev on 02 September 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "key-chain-manager.hpp"
#include <algorithm>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/face.hpp>
#include <ndnrtc/simple-log.h>
#include <boost/chrono.hpp>

using namespace boost::chrono;

using namespace ndn;

KeyChainManager::KeyChainManager(boost::shared_ptr<Face> face,
    const std::string& identityNameStr,
    const std::string& instanceNameStr,
    const std::string& configPolicy,
	unsigned int runTime):
face_(face),
signingIdentity_(identityNameStr),
instanceName_(instanceNameStr),
configPolicy_(configPolicy),
runTime_(runTime)
{
    checkExists(configPolicy);
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
		<< signingIdentity_  << " > signing.cert` if needed)" << std::endl;
}

void KeyChainManager::createMemoryKeychain()
{
    boost::shared_ptr<MemoryIdentityStorage> identityStorage = boost::make_shared<MemoryIdentityStorage>();
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage = boost::make_shared<MemoryPrivateKeyStorage>();
    instanceKeyChain_ = boost::make_shared<KeyChain>
      (boost::make_shared<IdentityManager>(identityStorage, privateKeyStorage),
       boost::make_shared<ConfigPolicyManager>(configPolicy_));
    instanceKeyChain_->setFace(face_.get());
}

void KeyChainManager::createInstanceIdentity()
{
	auto now = system_clock::now().time_since_epoch();
	duration<double> sec = now;

	Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();
    
    LogInfo("") << "Instance identity " << instanceIdentity << std::endl;

	Name instanceKeyName = instanceKeyChain_->generateRSAKeyPairAsDefault(instanceIdentity);
	Name signingCert = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(signingIdentity_));

	LogDebug("") << "Instance key " << instanceKeyName << std::endl;
	LogDebug("") << "Signing certificate " << signingCert << std::endl;

	std::vector<CertificateSubjectDescription> subjectDescriptions;
	instanceCert_ =
		instanceKeyChain_->getIdentityManager()->prepareUnsignedIdentityCertificate(instanceKeyName,
			Name(signingIdentity_),
			sec.count()*1000,
			(sec+duration<double>(runTime_)).count()*1000,
            subjectDescriptions,
            &instanceIdentity);
    assert(instanceCert_.get());
    
	defaultKeyChain_->sign(*instanceCert_, signingCert);
	instanceKeyChain_->installIdentityCertificate(*instanceCert_);
	instanceKeyChain_->setDefaultCertificateForKey(*instanceCert_);
    instanceKeyChain_->getIdentityManager()->setDefaultIdentity(instanceIdentity);

	LogInfo("") << "Instance certificate "
		<< instanceKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(instanceIdentity)) << std::endl;
}

void KeyChainManager::checkExists(const std::string& file)
{
    std::ifstream stream(file.c_str());
    bool result = (bool)stream;
    stream.close();
    
    if (!result)
    {
        std::stringstream ss;
        ss << "Can't find file " << file << std::endl;
        throw std::runtime_error(ss.str());
    }
}
