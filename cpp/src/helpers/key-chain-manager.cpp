//
// key-chain-manager.cpp
//
//  Created by Peter Gusev on 02 September 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "helpers/key-chain-manager.hpp"

#include <algorithm>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/file-private-key-storage.hpp>
#include <ndn-cpp/security/identity/basic-identity-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/security/v2/certificate-cache-v2.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/pib/pib-sqlite3.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-file.hpp>
#include <ndn-cpp/security/signing-info.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <ndn-cpp/face.hpp>
#include <boost/chrono.hpp>

using namespace boost::chrono;
using namespace ndn;
using namespace ndnrtc::helpers;

static const char *PublicDb = "pib.db";
static const char *PrivateDb = "ndnsec-key-file";

boost::shared_ptr<ndn::KeyChain> 
KeyChainManager::createKeyChain(std::string storagePath)
{
	boost::shared_ptr<KeyChain> keyChain;

	if (storagePath.size())
	{
		std::string privateKeysPath = std::string(storagePath) + "/" + std::string(PrivateDb);

#if SECURITY_V1
        std::string databaseFilePath = std::string(storagePath) + "/" + std::string(PublicDb);
		boost::shared_ptr<IdentityStorage> identityStorage = boost::make_shared<BasicIdentityStorage>(databaseFilePath);
		boost::shared_ptr<IdentityManager> identityManager = boost::make_shared<IdentityManager>(identityStorage, 
			boost::make_shared<FilePrivateKeyStorage>(privateKeysPath));
		boost::shared_ptr<PolicyManager> policyManager = boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get());

		keyChain = boost::make_shared<KeyChain>(identityManager, policyManager);
#else
        std::string databaseFilePath = std::string(storagePath);
        boost::shared_ptr<PibSqlite3> pib = boost::make_shared<PibSqlite3>(databaseFilePath);
        // pib->setTpmLocator();
        boost::shared_ptr<TpmBackEndFile> tpm = boost::make_shared<TpmBackEndFile>(privateKeysPath);

        keyChain = boost::make_shared<KeyChain>(pib, tpm);
#endif
	}
	else
	{
		keyChain = boost::make_shared<KeyChain>();
	}

	return keyChain;
}

KeyChainManager::KeyChainManager(boost::shared_ptr<ndn::Face> face,
                    boost::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    boost::shared_ptr<ndnlog::new_api::Logger> logger):
face_(face),
defaultKeyChain_(keyChain),
signingIdentity_(identityName),
instanceName_(instanceName),
configPolicy_(configPolicy),
runTime_(instanceCertLifetime)
{
	description_ = "key-chain-manager";
	setLogger(logger);

	if (configPolicy != "")
	{
		LogInfoC << "Setting up file-based policy manager from " << configPolicy << std::endl;

		checkExists(configPolicy);
		setupConfigPolicyManager();
	}
	else
	{
		LogInfoC << "Setting self-verify policy manager..." << std::endl;

		identityStorage_ = boost::make_shared<MemoryIdentityStorage>();
		privateKeyStorage_ = boost::make_shared<MemoryPrivateKeyStorage>();
		configPolicyManager_ = boost::make_shared<SelfVerifyPolicyManager>(identityStorage_.get());
	}

	setupInstanceKeyChain();
}

void KeyChainManager::publishCertificates()
{
	memoryContentCache_ = boost::make_shared<MemoryContentCache>(face_.get());

	{ // registering prefix for serving signing identity: <signing-identity>/KEY
		Name prefix(signingIdentity_);
		prefix.append("KEY");
		registerPrefix(prefix);
	}

	{ // registering prefix for serving instance identity: <instance-identity>/KEY
		Name prefix(signingIdentity_);
		prefix.append(instanceName_).append("KEY");
		registerPrefix(prefix);
	}

	memoryContentCache_->add(*(this->signingIdentityCertificate()));
	memoryContentCache_->add(*(this->instanceCertificate()));
}

//******************************************************************************
void KeyChainManager::setupDefaultKeyChain()
{
    defaultKeyChain_ = boost::make_shared<ndn::KeyChain>();
}

void KeyChainManager::setupInstanceKeyChain()
{
    Name signingIdentity(signingIdentity_);
    std::vector<Name> identities;

    if (defaultKeyChain_->getIsSecurityV1())
    {
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, false);
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, true);
    }
    else
        defaultKeyChain_->getPib().getAllIdentityNames(identities);

    if (std::find(identities.begin(), identities.end(), signingIdentity) == identities.end())
    {
        // create signing identity in default keychain
        LogInfo("") << "Signing identity not found. Creating..." << std::endl;
        createSigningIdentity();
    }

	if (defaultKeyChain_->getIsSecurityV1()) {
		Name certName = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(signingIdentity);
		signingCert_ = defaultKeyChain_->getCertificate(certName);
	}
	else
	{
		Name certName = defaultKeyChain_->getPib().getIdentity(signingIdentity)->getDefaultKey()->getDefaultCertificate()->getName();
		signingCert_ = defaultKeyChain_->getPib().getIdentity(CertificateV2::extractIdentityFromCertName(certName))
				->getKey(CertificateV2::extractKeyNameFromCertName(certName))
    			->getCertificate(certName);
	}

	face_->setCommandSigningInfo(*defaultKeyChain_, signingCert_->getName());

	createMemoryKeychain();
	if (defaultKeyChain_->getIsSecurityV1())
		createInstanceIdentity();
	else
		createInstanceIdentityV2();
}

void KeyChainManager::setupConfigPolicyManager()
{
    identityStorage_ = boost::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = boost::make_shared<MemoryPrivateKeyStorage>();
    if (defaultKeyChain_->getIsSecurityV1())
    {
        configPolicyManager_ = boost::make_shared<ConfigPolicyManager>(configPolicy_);
    }
    else
    {
        configPolicyManager_ = boost::make_shared<ConfigPolicyManager>(configPolicy_, boost::make_shared<CertificateCacheV2>());
    }
}

void KeyChainManager::createSigningIdentity()
{
    // create self-signed certificate
    Name cert = defaultKeyChain_->createIdentityAndCertificate(Name(signingIdentity_));

	LogInfoC << "Generated identity " << signingIdentity_ << " (certificate name " 
		<< cert << ")" << std::endl;
	LogInfoC << "Check policy config file for correct trust anchor (run `ndnsec-dump-certificate -i " 
		<< signingIdentity_  << " > signing.cert` if needed)" << std::endl;
}

void KeyChainManager::createMemoryKeychain()
{
    if (defaultKeyChain_->getIsSecurityV1())
        instanceKeyChain_ = boost::make_shared<KeyChain>(boost::make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
                                                         configPolicyManager_);
    else
        instanceKeyChain_ = boost::make_shared<KeyChain>(boost::make_shared<PibMemory>(), boost::make_shared<TpmBackEndMemory>(),
                                                         configPolicyManager_);

    instanceKeyChain_->setFace(face_.get());
}

void KeyChainManager::createInstanceIdentity()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();
    
    LogInfoC << "Instance identity " << instanceIdentity << std::endl;

    Name instanceKeyName = instanceKeyChain_->generateRSAKeyPairAsDefault(instanceIdentity, true);
    Name signingCert = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(signingIdentity_));

	LogDebugC << "Instance key " << instanceKeyName << std::endl;
	LogDebugC << "Signing certificate " << signingCert << std::endl;

    std::vector<CertificateSubjectDescription> subjectDescriptions;
    instanceCert_ =
        instanceKeyChain_->getIdentityManager()->prepareUnsignedIdentityCertificate(instanceKeyName,
                                                                                    Name(signingIdentity_),
                                                                                    sec.count() * 1000,
                                                                                    (sec + duration<double>(runTime_)).count() * 1000,
                                                                                    subjectDescriptions,
                                                                                    &instanceIdentity);
    assert(instanceCert_.get());

    defaultKeyChain_->sign(*instanceCert_, signingCert);
    instanceKeyChain_->installIdentityCertificate(*instanceCert_);
    instanceKeyChain_->setDefaultCertificateForKey(*instanceCert_);
    instanceKeyChain_->getIdentityManager()->setDefaultIdentity(instanceIdentity);

	LogInfoC << "Instance certificate "
		<< instanceKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(instanceIdentity)) << std::endl;
}

void KeyChainManager::createInstanceIdentityV2()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();
    
    LogInfoC << "Instance identity " << instanceIdentity << std::endl;

	boost::shared_ptr<PibIdentity> instancePibIdentity =
      instanceKeyChain_->createIdentityV2(instanceIdentity);
	boost::shared_ptr<PibKey> instancePibKey = 
      instancePibIdentity->getDefaultKey();
	boost::shared_ptr<PibKey> signingPibKey = defaultKeyChain_->getPib()
      .getIdentity(Name(signingIdentity_))->getDefaultKey();
	Name signingCert = signingPibKey->getDefaultCertificate()->getName();

	LogDebugC << "Instance key " << instancePibKey->getName() << std::endl;
	LogDebugC << "Signing certificate " << signingCert << std::endl;

    // Prepare the instance certificate.
    boost::shared_ptr<CertificateV2> instanceCertificate(new CertificateV2());
    Name certificateName = instancePibKey->getName();
    // Use the issuer's public key digest.
    certificateName.append(PublicKey(signingPibKey->getPublicKey()).getDigest());
    certificateName.appendVersion((uint64_t)(sec.count() * 1000));
    instanceCertificate->setName(certificateName);
    instanceCertificate->getMetaInfo().setType(ndn_ContentType_KEY);
    instanceCertificate->getMetaInfo().setFreshnessPeriod(3600 * 1000.0);
    instanceCertificate->setContent(instancePibKey->getPublicKey());

    SigningInfo signingParams(signingPibKey);
    signingParams.setValidityPeriod(ValidityPeriod(sec.count() * 1000,
                                                   (sec + duration<double>(runTime_)).count() * 1000));
    defaultKeyChain_->sign(*instanceCertificate, signingParams);

    instanceKeyChain_->addCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultIdentity(*instancePibIdentity);
    instanceCert_ = instanceCertificate;

	LogInfoC << "Instance certificate "
		<< instanceKeyChain_->getPib().getIdentity(Name(instanceIdentity_))
          ->getDefaultKey()->getDefaultCertificate()->getName() << std::endl;
}

void KeyChainManager::registerPrefix(const Name& prefix)
{
	boost::shared_ptr<MemoryContentCache> memCache = memoryContentCache_;
	boost::shared_ptr<ndnlog::new_api::Logger> logger = logger_;

	memoryContentCache_->registerPrefix(prefix,
		[logger](const boost::shared_ptr<const Name> &p){
			if (logger) logger->log(ndnlog::NdnLoggerLevelError) << "Prefix registration failure: " << p->toUri() << std::endl;
		},
		[logger](const boost::shared_ptr<const Name>& p, uint64_t id){
			if (logger)  logger->log(ndnlog::NdnLoggerLevelInfo) << "Prefix registration success: " << p->toUri() << std::endl;
		},
		[logger, memCache](const boost::shared_ptr<const Name>& p,
			const boost::shared_ptr<const Interest> &i,
			Face& f, uint64_t, const boost::shared_ptr<const InterestFilter>&){
			if (logger) logger->log(ndnlog::NdnLoggerLevelWarning) << "Unexpected interest received " << i->getName() 
			<< std::endl;

			memCache->storePendingInterest(i, f);
		});
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
