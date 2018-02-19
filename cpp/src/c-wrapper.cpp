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
#include <cstring>

#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/security/identity/basic-identity-storage.hpp>
#include <ndn-cpp/security/identity/file-private-key-storage.hpp>
#include <ndn-cpp/security/v2/certificate-cache-v2.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>
#include <ndn-cpp/security/signing-info.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include "face-processor.hpp"
#include "local-stream.hpp"
#include "simple-log.hpp"

using namespace ndn;
using namespace ndnrtc;
using namespace boost::chrono;
using namespace ndnlog::new_api;

class KeyChainManager;

static const char *PublicDb = "public-info.db";
static const char *PrivateDb = "keys";

static boost::shared_ptr<FaceProcessor> LibFaceProcessor;
static boost::shared_ptr<MemoryContentCache> LibContentCache; // for registering a prefix and storing certs
static boost::shared_ptr<KeyChain> LibKeyChain;
static boost::shared_ptr<KeyChainManager> LibKeyChainManager;

void initKeyChain(const char* storagePath, std::string signingIdentity);
void initFace(std::string hostname, boost::shared_ptr<Logger> logger,
	std::string configPolicyFilePath, std::string signingIdentity, std::string instanceId);
MediaStreamParams prepareMediaStreamParams(LocalStreamParams params);
void registerPrefix(Name prefix, boost::shared_ptr<Logger> logger);

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

//******************************************************************************
const char* ndnrtc_lib_version()
{
	return PACKAGE_VERSION;
}

void ndnrtc_get_identities_list(char*** identities, int* nIdentities)
{
	boost::shared_ptr<ndn::KeyChain> defaultKeyChain = boost::make_shared<ndn::KeyChain>();
	std::vector<Name> identitiesList;

	if (defaultKeyChain->getIsSecurityV1()) {
		defaultKeyChain->getIdentityManager()->getAllIdentities(identitiesList, false);
		defaultKeyChain->getIdentityManager()->getAllIdentities(identitiesList, true);
	}
    else
		defaultKeyChain->getPib().getAllIdentityNames(identitiesList);

	*nIdentities = identitiesList.size();
	*identities = (char**)malloc(sizeof(char*)*identitiesList.size());
	
	int idx = 0;
	for (auto identity:identitiesList)
	{
		(*identities)[idx] = (char*)malloc(identity.toUri().size()+1);
		memset((*identities)[idx], 0, identity.toUri().size()+1);
		strcpy((*identities)[idx++], identity.toUri().c_str());
	}
}

bool ndnrtc_init(const char* hostname, const char* storagePath, 
	const char* policyFilePath, const char* signingIdentity,
	const char * instanceId, LibLog libLog)
{
	Logger::initAsyncLogging();

	boost::shared_ptr<Logger> callbackLogger = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelAll,
		boost::make_shared<CallbackSink>(libLog));
	callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up NDN-RTC..." << std::endl;

	try {
		initKeyChain(storagePath, signingIdentity);
		initFace(hostname, callbackLogger, (policyFilePath ? policyFilePath : ""), signingIdentity, instanceId);
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

ndnrtc::IStream* ndnrtc_createLocalStream(LocalStreamParams params, LibLog loggerSink)
{
	if (params.typeIsVideo == 1)
	{
		MediaStreamSettings settings(LibFaceProcessor->getIo(), prepareMediaStreamParams(params));
		settings.sign_ = (params.signingOn == 1);
		settings.face_ = LibFaceProcessor->getFace().get();
		settings.keyChain_ = LibKeyChainManager->instanceKeyChain().get();

		boost::shared_ptr<Logger> callbackLogger = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelNone,
			boost::make_shared<CallbackSink>(loggerSink));
		callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up Local Video Stream with params ("
			<< "signing " << (settings.sign_ ? "ON" : "OFF")
	 		<< "):" << settings.params_ << std::endl;

		LocalVideoStream *stream = new LocalVideoStream(params.basePrefix, settings, (params.fecOn == 1));
		stream->setLogger(callbackLogger);

		// registering prefix for the stream
		Name prefix(stream->getPrefix());
		registerPrefix(prefix, callbackLogger);

		return stream;
	}

	return nullptr;
}

void ndnrtc_destroyLocalStream(ndnrtc::IStream* localStreamObject)
{
	if (localStreamObject)
		delete localStreamObject;
}

// const char* ndnrtc_LocalStream_getPrefix(IStream *stream)
void ndnrtc_LocalStream_getPrefix(ndnrtc::IStream *stream, char *prefix)
{
	if (stream)
		strcpy(prefix, stream->getPrefix().c_str());
}


void ndnrtc_LocalStream_getBasePrefix(ndnrtc::IStream *stream, char* basePrefix)
{
	if (stream)
		strcpy(basePrefix, stream->getBasePrefix().c_str());
}

void ndnrtc_LocalStream_getStreamName(ndnrtc::IStream *stream, char* streamName)
{
	if (stream)
		strcpy(streamName, stream->getStreamName().c_str());
}

int ndnrtc_LocalVideoStream_incomingI420Frame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideU,
			const unsigned int strideV,
			const unsigned char* yBuffer,
			const unsigned char* uBuffer,
			const unsigned char* vBuffer)
{
	if (stream)
		return stream->incomingI420Frame(width, height, strideY, strideU, strideV, yBuffer, uBuffer, vBuffer);
	return -1;
}

int ndnrtc_LocalVideoStream_incomingNV21Frame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideUV,
			const unsigned char* yBuffer,
			const unsigned char* uvBuffer)
{
	if (stream)
		return stream->incomingNV21Frame(width, height, strideY, strideUV, yBuffer, uvBuffer);
	return -1;
}

int ndnrtc_LocalVideoStream_incomingArgbFrame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			unsigned char* argbFrameData,
			unsigned int frameSize)
{
	if (stream)
		return stream->incomingArgbFrame(width, height, argbFrameData, frameSize);
	return -1;
}
//******************************************************************************
// private
// initializes new file-based (or default) keychain
// if signing identity does not exist, creates it
void initKeyChain(const char* storagePath, std::string signingIdentityStr)
{
	if (storagePath)
	{
		std::string databaseFilePath = std::string(storagePath) + "/" + std::string(PublicDb);
		std::string privateKeysPath = std::string(storagePath) + "/" + std::string(PrivateDb);

		boost::shared_ptr<IdentityStorage> identityStorage = boost::make_shared<BasicIdentityStorage>(databaseFilePath);
		boost::shared_ptr<IdentityManager> identityManager = boost::make_shared<IdentityManager>(identityStorage, 
			boost::make_shared<FilePrivateKeyStorage>(privateKeysPath));
		boost::shared_ptr<PolicyManager> policyManager = boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get());

		LibKeyChain = boost::make_shared<KeyChain>(identityManager, policyManager);
		const Name signingIdentity = Name(signingIdentityStr);
		LibKeyChain->createIdentityAndCertificate(signingIdentity);
		identityManager->setDefaultIdentity(signingIdentity);
	}
	else
	{
		LibKeyChain = boost::make_shared<KeyChain>();
	}
}

// initializes face and face processing thread 
void initFace(std::string hostname, boost::shared_ptr<Logger> logger, 
	std::string configPolicyFilePath, std::string signingIdentityStr, 
	std::string instanceIdStr)
{
	LibFaceProcessor = boost::make_shared<FaceProcessor>(hostname);
	LibKeyChainManager = boost::make_shared<KeyChainManager>(LibFaceProcessor->getFace(),
		LibKeyChain, 
		signingIdentityStr,
		instanceIdStr,
		configPolicyFilePath,
		3600,
		logger);

	LibFaceProcessor->performSynchronized([logger](boost::shared_ptr<Face> face){
		logger->log(ndnlog::NdnLoggerLevelInfo) << "Setting command signing info with certificate: " 
			<< LibKeyChainManager->signingIdentityCertificate()->getName() << std::endl;

		face->setCommandSigningInfo(*(LibKeyChainManager->defaultKeyChain()),
			LibKeyChainManager->signingIdentityCertificate()->getName());
		LibContentCache = boost::make_shared<MemoryContentCache>(face.get());
	});

	{ // registering prefix for serving signing ifdentity: <signing-identity>/KEY
		Name prefix(signingIdentityStr);
		prefix.append("KEY");
		registerPrefix(prefix, logger);
	}

	{ // registering prefix for serving instance ifdentity: <instance-identity>/KEY
		Name prefix(signingIdentityStr);
		prefix.append(instanceIdStr).append("KEY");
		registerPrefix(prefix, logger);
	}

	LibContentCache->add(*(LibKeyChainManager->signingIdentityCertificate()));
	LibContentCache->add(*(LibKeyChainManager->instanceCertificate()));
}

void registerPrefix(Name prefix, boost::shared_ptr<Logger> logger) 
{
	LibContentCache->registerPrefix(prefix,
		[logger](const boost::shared_ptr<const Name> &p){
			logger->log(ndnlog::NdnLoggerLevelError) << "Prefix registration failure: " << p << std::endl;
		},
		[logger](const boost::shared_ptr<const Name>& p, uint64_t id){
			logger->log(ndnlog::NdnLoggerLevelInfo) << "Prefix registration success: " << p << std::endl;
		},
		[logger](const boost::shared_ptr<const Name>& p,
			const boost::shared_ptr<const Interest> &i,
			Face& f, uint64_t, const boost::shared_ptr<const InterestFilter>&){
			logger->log(ndnlog::NdnLoggerLevelWarning) << "Unexpected interest received " << i->getName() 
				<< std::endl;

			LibContentCache->storePendingInterest(i, f);
		});
}

//******************************************************************************
MediaStreamParams prepareMediaStreamParams(LocalStreamParams params)
{

	MediaStreamParams p(params.streamName);
	p.producerParams_.segmentSize_ = params.ndnSegmentSize;
	p.producerParams_.freshnessMs_ = params.ndnDataFreshnessPeriodMs;

	if (params.typeIsVideo == 1)
	{
		p.type_ = MediaStreamParams::MediaStreamTypeVideo;

		VideoCoderParams vcp;
		vcp.codecFrameRate_ = 30;
		vcp.gop_ = params.gop;
		vcp.startBitrate_ = params.startBitrate;
		vcp.maxBitrate_ = params.maxBitrate;
		vcp.encodeWidth_ = params.frameWidth;
		vcp.encodeHeight_ = params.frameHeight;
		vcp.dropFramesOn_ = (params.dropFrames == 1);

		VideoThreadParams vp(params.threadName, vcp);
		p.addMediaThread(vp);
	}

	return p;
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

void KeyChainManager::setupDefaultKeyChain()
{
	defaultKeyChain_ = boost::make_shared<ndn::KeyChain>();
}

void KeyChainManager::setupInstanceKeyChain()
{
	Name signingIdentity(signingIdentity_);
	std::vector<Name> identities;

	if (defaultKeyChain_->getIsSecurityV1()) {
		defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, false);
		defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, true);
		
	}
    else
		defaultKeyChain_->getPib().getAllIdentityNames(identities);

	if (std::find(identities.begin(), identities.end(), signingIdentity) == identities.end())
	{
		// create signing identity in default keychain
		LogInfoC << "Signing identity not found. Creating..." << std::endl;
		createSigningIdentity();
	}

	// TODO update for V2
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
		configPolicyManager_ = boost::make_shared<ConfigPolicyManager>(configPolicy_);
	else
		configPolicyManager_ = boost::make_shared<ConfigPolicyManager>
          (configPolicy_, boost::make_shared<CertificateCacheV2>());
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
        instanceKeyChain_ = boost::make_shared<KeyChain>
          (boost::make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
           configPolicyManager_);
    else
        instanceKeyChain_ = boost::make_shared<KeyChain>
          (boost::make_shared<PibMemory>(), boost::make_shared<TpmBackEndMemory>(),
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
    certificateName.appendVersion((uint64_t)(sec.count()*1000));
	instanceCertificate->setName(certificateName);
	instanceCertificate->getMetaInfo().setType(ndn_ContentType_KEY);
	instanceCertificate->getMetaInfo().setFreshnessPeriod(3600 * 1000.0);
	instanceCertificate->setContent(instancePibKey->getPublicKey());

    SigningInfo signingParams(signingPibKey);
	signingParams.setValidityPeriod
      (ValidityPeriod(sec.count() * 1000, 
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

