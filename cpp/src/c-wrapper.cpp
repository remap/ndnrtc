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
#include <ndn-cpp/util/memory-content-cache.hpp>

#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/shared_ptr.hpp>
#include <memory>

#include "face-processor.hpp"
#include "local-stream.hpp"
#include "simple-log.hpp"
#include "name-components.hpp"
#include "frame-fetcher.hpp"

using namespace ndn;
using namespace ndnrtc;
using namespace boost::chrono;
using namespace ndnlog::new_api;

namespace cwrapper_tools {
class KeyChainManager;
}
using namespace cwrapper_tools;

static const char *PublicDb = "public-info.db";
static const char *PrivateDb = "private-keys";

static std::shared_ptr<FaceProcessor> LibFaceProcessor;
static std::shared_ptr<MemoryContentCache> LibContentCache; // for registering a prefix and storing certs
static std::shared_ptr<KeyChain> LibKeyChain;
static std::shared_ptr<KeyChainManager> LibKeyChainManager;

void initKeyChain(std::string storagePath, std::string signingIdentity);
void initFace(std::string hostname, std::shared_ptr<Logger> logger,
	std::string signingIdentity, std::string instanceId);
MediaStreamParams prepareMediaStreamParams(LocalStreamParams params);
void registerPrefix(Name prefix, std::shared_ptr<Logger> logger);

//******************************************************************************
namespace cwrapper_tools {
class KeyChainManager : public ndnlog::new_api::ILoggingObject {
public:
    KeyChainManager(std::shared_ptr<ndn::Face> face,
                    std::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    std::shared_ptr<Logger> logger);

	std::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
	std::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }
    std::string instancePrefix() const { return instanceIdentity_; }
    
    const std::shared_ptr<ndn::IdentityCertificate> instanceCertificate() const
        { return instanceCert_; }

    const std::shared_ptr<ndn::IdentityCertificate> signingIdentityCertificate() const
    {
    	return signingCert_;
    }
    
private:
    std::shared_ptr<ndn::Face> face_;
	std::string signingIdentity_, instanceName_,
        configPolicy_, instanceIdentity_;
	unsigned int runTime_;

    std::shared_ptr<ndn::PolicyManager> configPolicyManager_;
	std::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;
    std::shared_ptr<ndn::IdentityCertificate> instanceCert_, signingCert_;
    std::shared_ptr<ndn::IdentityStorage> identityStorage_;
    std::shared_ptr<ndn::PrivateKeyStorage> privateKeyStorage_;

	void setupDefaultKeyChain();
	void setupInstanceKeyChain();
    void setupConfigPolicyManager();
	void createSigningIdentity();
	void createMemoryKeychain();
	void createInstanceIdentity();
    void checkExists(const std::string&);
};
}

//******************************************************************************
const char* ndnrtc_getVersion()
{
    static std::string version = NameComponents::fullVersion();
    return version.c_str();
}

bool ndnrtc_init(const char* hostname, const char* storagePath, 
	const char* signingIdentity, const char * instanceId, LibLog libLog)
{
	Logger::initAsyncLogging();

	std::shared_ptr<Logger> callbackLogger = std::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelAll,
		std::make_shared<CallbackSink>(libLog));
	callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up NDN-RTC..." << std::endl;

	try {
		initKeyChain(storagePath, signingIdentity);
		initFace(hostname, callbackLogger, signingIdentity, instanceId);
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
        settings.storagePath_ = (params.storagePath ? std::string(params.storagePath) : "");

		std::shared_ptr<Logger> callbackLogger = std::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelNone,
			std::make_shared<CallbackSink>(loggerSink));
		callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up Local Video Stream with params ("
			<< "signing " << (settings.sign_ ? "ON" : "OFF")
	 		<< "):" << settings.params_ << std::endl;

		LocalVideoStream *stream = new LocalVideoStream(params.basePrefix, settings, (params.fecOn == 1));
		stream->setLogger(callbackLogger);

		// registering prefix for the stream
		Name prefix(stream->getPrefix());
		registerPrefix(prefix.getPrefix(-1), callbackLogger);

		return stream;
	}

	return nullptr;
}


cFrameInfo ndnrtc_LocalVideoStream_getLastPublishedInfo(ndnrtc::LocalVideoStream *stream)
{
    static char *frameName = nullptr;
    if (!frameName) frameName = (char*)malloc(1024*sizeof(char));

    memset((void*)frameName, 0, 1024);
    cFrameInfo frameInfo({0, 0, frameName});

    if (stream)
    {
        std::map<std::string, FrameInfo> lastPublishedInfo = stream->getLastPublishedInfo();
        if (lastPublishedInfo.size())
        {
            FrameInfo fi = lastPublishedInfo.begin()->second;
            frameInfo.timestamp_ = fi.timestamp_;
            frameInfo.playbackNo_ = fi.playbackNo_;
            strcpy(frameInfo.ndnName_, fi.ndnName_.c_str());
        }
    }

    return frameInfo;
}

void ndnrtc_destroyLocalStream(ndnrtc::IStream* localStreamObject)
{
	if (localStreamObject)
		delete localStreamObject;
}

const char* ndnrtc_LocalStream_getPrefix(IStream *stream)
{
	if (stream)
		return stream->getPrefix().c_str();
	return "n/a";
}

const char* ndnrtc_LocalStream_getBasePrefix(IStream *stream)
{
	if (stream)
		return stream->getBasePrefix().c_str();
	return "n/a";
}

const char* ndnrtc_LocalStream_getStreamName(IStream *stream)
{
	if (stream)
		return stream->getStreamName().c_str();
	return "n/a";
}

double ndnrtc_getStatistic(ndnrtc::IStream *stream, const char* statName)
{
    if (stream)
    {
        bool f = false;
        statistics::Indicator ind;
        std::string sname(statName);
        for (auto p:statistics::StatisticsStorage::IndicatorKeywords)
            if (p.second == sname)
            {
                ind = p.first;
                f = true;
                break;
            }

        if (f)
        {
            statistics::StatisticsStorage::StatRepo repo = stream->getStatistics().getIndicators();
            if (repo.find(ind) != repo.end())
                return repo[ind];
        }
        return 0;
    }

    return 0;
}

static std::map<std::string, std::shared_ptr<FrameFetcher>> FrameFetchers;
void ndnrtc_FrameFetcher_fetch(ndnrtc::IStream *stream,
                               const char* frameName, 
                               BufferAlloc bufferAllocFunc,
                               FrameFetched frameFetchedFunc)
{
    std::shared_ptr<StorageEngine> storage = ((LocalVideoStream*)stream)->getStorage();
    std::shared_ptr<FrameFetcher> ff = std::make_shared<FrameFetcher>(storage);

    std::string fkey(frameName);
    FrameFetchers[fkey] = ff;

    ((LocalVideoStream*)stream)->getLogger()->log(ndnlog::NdnLoggerLevelInfo) << "Setting up frame-fetcher for " << fkey << std::endl;

    ff->setLogger(((LocalVideoStream*)stream)->getLogger());
    ff->fetch(Name(frameName),
              [fkey, bufferAllocFunc](const std::shared_ptr<IFrameFetcher>& fetcher, 
                                      int width, int height)->uint8_t*
              {
                  return bufferAllocFunc(fkey.c_str(), width, height);
              },
              [fkey, frameFetchedFunc](const std::shared_ptr<IFrameFetcher>& fetcher, 
                 const FrameInfo fi, int nFetchedFrames,
                 int width, int height, const uint8_t* buffer){
                    cFrameInfo frameInfo({fi.timestamp_, fi.playbackNo_, (char*)fi.ndnName_.c_str()});
                    frameFetchedFunc(frameInfo, width, height, buffer);
                    FrameFetchers.erase(fkey);
              },
              [fkey, frameFetchedFunc](const std::shared_ptr<IFrameFetcher>& ff, std::string reason){
                    cFrameInfo frameInfo({0,0,(char*)fkey.c_str()});
                    frameFetchedFunc(frameInfo, 0, 0, nullptr);
                    FrameFetchers.erase(fkey);
              });
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
// initializes new file-based keychain
// if signing identity does not exist, creates it
void initKeyChain(std::string storagePath, std::string signingIdentityStr)
{
	std::string databaseFilePath = storagePath + "/" + std::string(PublicDb);
	std::string privateKeysPath = storagePath + "/" + std::string(PrivateDb);

	std::shared_ptr<IdentityStorage> identityStorage = std::make_shared<BasicIdentityStorage>(databaseFilePath);
	std::shared_ptr<IdentityManager> identityManager = std::make_shared<IdentityManager>(identityStorage, 
		std::make_shared<FilePrivateKeyStorage>(privateKeysPath));
	std::shared_ptr<PolicyManager> policyManager = std::make_shared<SelfVerifyPolicyManager>(identityStorage.get());

	LibKeyChain = std::make_shared<KeyChain>(identityManager, policyManager);

	const Name signingIdentity = Name(signingIdentityStr);
	LibKeyChain->createIdentityAndCertificate(signingIdentity);
	identityManager->setDefaultIdentity(signingIdentity);
}

// initializes face and face processing thread 
void initFace(std::string hostname, std::shared_ptr<Logger> logger, 
	std::string signingIdentityStr, std::string instanceIdStr)
{
	LibFaceProcessor = std::make_shared<FaceProcessor>(hostname);
	LibKeyChainManager = std::make_shared<KeyChainManager>(LibFaceProcessor->getFace(),
		LibKeyChain, 
		std::string(signingIdentityStr),
		std::string(instanceIdStr),
		std::string("policy-file.conf"),
		3600,
		logger);

	LibFaceProcessor->performSynchronized([logger](std::shared_ptr<Face> face){
		logger->log(ndnlog::NdnLoggerLevelInfo) << "Setting command signing info with certificate: " 
			<< LibKeyChainManager->defaultKeyChain()->getDefaultCertificateName() << std::endl;

		face->setCommandSigningInfo(*(LibKeyChainManager->defaultKeyChain()),
			LibKeyChainManager->defaultKeyChain()->getDefaultCertificateName());
		LibContentCache = std::make_shared<MemoryContentCache>(face.get());
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

void registerPrefix(Name prefix, std::shared_ptr<Logger> logger) 
{
	LibContentCache->registerPrefix(prefix,
		[logger](const std::shared_ptr<const Name> &p){
			logger->log(ndnlog::NdnLoggerLevelError) << "Prefix registration failure: " << p << std::endl;
		},
		[logger](const std::shared_ptr<const Name>& p, uint64_t id){
			logger->log(ndnlog::NdnLoggerLevelInfo) << "Prefix registration success: " << p << std::endl;
		},
		[logger](const std::shared_ptr<const Name>& p,
			const std::shared_ptr<const Interest> &i,
			Face& f, uint64_t, const std::shared_ptr<const InterestFilter>&){
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
	p.producerParams_.freshness_= {10, 30, 900};

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
KeyChainManager::KeyChainManager(std::shared_ptr<ndn::Face> face,
                    std::shared_ptr<ndn::KeyChain> keyChain,
                    const std::string& identityName,
                    const std::string& instanceName,
                    const std::string& configPolicy,
                    unsigned int instanceCertLifetime,
                    std::shared_ptr<Logger> logger):
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
	identityStorage_ = std::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = std::make_shared<MemoryPrivateKeyStorage>();
	configPolicyManager_ = std::make_shared<SelfVerifyPolicyManager>(identityStorage_.get());
	setupInstanceKeyChain();
}

void KeyChainManager::setupDefaultKeyChain()
{
	defaultKeyChain_ = std::make_shared<ndn::KeyChain>();
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

	Name certName = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(signingIdentity);
	signingCert_ = defaultKeyChain_->getCertificate(certName);

	createMemoryKeychain();
	createInstanceIdentity();
}

void KeyChainManager::setupConfigPolicyManager()
{
	identityStorage_ = std::make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = std::make_shared<MemoryPrivateKeyStorage>();
	configPolicyManager_ = std::make_shared<ConfigPolicyManager>(configPolicy_);
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
    instanceKeyChain_ = std::make_shared<KeyChain>
      (std::make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
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

