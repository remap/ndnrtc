// 
// c-wrapper.cpp
//
// Copyright (c) 2017 Regents of the University of California
// For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "c-wrapper.h"

#include <algorithm>

#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/security/identity/basic-identity-storage.hpp>
#include <ndn-cpp/security/identity/file-private-key-storage.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include "face-processor.hpp"
#include "local-stream.hpp"
#include "simple-log.hpp"

#define SIGNING_IDENTITY "/ice-ar"

using namespace ndn;
using namespace ndnrtc;
using namespace boost::chrono;
using namespace ndnlog::new_api;

class KeyChainManager;

static const char *PublicDb = "public-info.db";
static const char *PrivateDb = "private-keys";

static boost::shared_ptr<FaceProcessor> LibFaceProcessor;
static boost::shared_ptr<KeyChain> LibKeyChain;
static boost::shared_ptr<KeyChainManager> LibKeyChainManager;

void initKeyChain(std::string storagePath);
void initFace(std::string hostname, boost::shared_ptr<Logger> logger);

//******************************************************************************
class KeyChainManager : public ndnlog::new_api::ILoggingObject {
public:
    KeyChainManager(boost::shared_ptr<ndn::Face> face,
                    boost::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    boost::shared_ptr<Logger> logger);

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

    boost::shared_ptr<ndn::PolicyManager> configPolicyManager_;
	boost::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;
    boost::shared_ptr<ndn::IdentityCertificate> instanceCert_;
    boost::shared_ptr<ndn::IdentityStorage> identityStorage_;
    boost::shared_ptr<ndn::PrivateKeyStorage> privateKeyStorage_;

	void setupDefaultKeyChain();
	void setupInstanceKeyChain();
    void setupConfigPolicyManager();
	void createSigningIdentity();
	void createMemoryKeychain();
	void createInstanceIdentity();
    void checkExists(const std::string&);
};

//******************************************************************************
bool ndnrtc_init(const char* hostname, const char* storagePath, LibLog libLog)
{
	Logger::initAsyncLogging();

	boost::shared_ptr<Logger> callbackLogger = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelAll,
		boost::make_shared<CallbackSink>(libLog));
	callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up NDN-RTC..." << std::endl;

	try {
		initKeyChain(std::string(storagePath));
		initFace(std::string(hostname), callbackLogger);
	}
	catch (std::exception &e)
	{
		std::stringstream ss;
		ss << "setup error: " << e.what() << std::endl;
		libLog(ss.str().c_str());
		return false;
	}
	return true;
}

void ndnrtc_deinit()
{
	if (LibFaceProcessor)
		LibFaceProcessor->stop();
	LibFaceProcessor.reset();
	LibKeyChainManager.reset();
	LibKeyChain.reset();
	Logger::releaseAsyncLogging();
}

//******************************************************************************
// private
// initializes new file-based keychain
// if signing identity does not exist, creates it
void initKeyChain(std::string storagePath)
{
	std::string databaseFilePath = storagePath + "/" + std::string(PublicDb);
	std::string privateKeysPath = storagePath + "/" + std::string(PrivateDb);

	boost::shared_ptr<IdentityStorage> identityStorage = boost::make_shared<BasicIdentityStorage>(databaseFilePath);
	boost::shared_ptr<IdentityManager> identityManager = boost::make_shared<IdentityManager>(identityStorage, 
		boost::make_shared<FilePrivateKeyStorage>(privateKeysPath));
	boost::shared_ptr<PolicyManager> policyManager = boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get());

	LibKeyChain = boost::make_shared<KeyChain>(identityManager, policyManager);

	const Name signingIdentity = Name(SIGNING_IDENTITY);
	LibKeyChain->createIdentityAndCertificate(signingIdentity);
	identityManager->setDefaultIdentity(signingIdentity);
}

// initializes face and face processing thread 
void initFace(std::string hostname, boost::shared_ptr<Logger> logger)
{
	LibFaceProcessor = boost::make_shared<FaceProcessor>(hostname);
	LibKeyChainManager = boost::make_shared<KeyChainManager>(LibFaceProcessor->getFace(),
		LibKeyChain, 
		std::string(SIGNING_IDENTITY),
		std::string("mobile-terminal0"),
		std::string("policy-file.conf"),
		3600,
		logger);

	LibFaceProcessor->dispatchSynchronized([logger](boost::shared_ptr<Face> face){
		logger->log(ndnlog::NdnLoggerLevelInfo) << "Setting command signing info with certificate: " 
			<< LibKeyChainManager->defaultKeyChain()->getDefaultCertificateName() << std::endl;

		face->setCommandSigningInfo(*(LibKeyChainManager->defaultKeyChain()),
			LibKeyChainManager->defaultKeyChain()->getDefaultCertificateName());
	});
}

//******************************************************************************
KeyChainManager::KeyChainManager(boost::shared_ptr<ndn::Face> face,
                    boost::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    boost::shared_ptr<Logger> logger):
face_(face),
defaultKeyChain_(keyChain),
signingIdentity_(identityName),
instanceName_(instanceName),
configPolicy_(configPolicy),
runTime_(instanceCertLifetime)
{
	description_ = "key-chain-manager";
	setLogger(logger);
	// checkExists(configPolicy);
	identityStorage_ = boost::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = boost::make_shared<MemoryPrivateKeyStorage>();
	configPolicyManager_ = boost::make_shared<SelfVerifyPolicyManager>(identityStorage_.get());
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
		LogInfoC << "Signing identity not found. Creating..." << std::endl;
		createSigningIdentity();
	}

	createMemoryKeychain();
	createInstanceIdentity();
}

void KeyChainManager::setupConfigPolicyManager()
{
	identityStorage_ = boost::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = boost::make_shared<MemoryPrivateKeyStorage>();
	configPolicyManager_ = boost::make_shared<ConfigPolicyManager>(configPolicy_);
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
    instanceKeyChain_ = boost::make_shared<KeyChain>
      (boost::make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
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
			sec.count()*1000,
			(sec+duration<double>(runTime_)).count()*1000,
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

