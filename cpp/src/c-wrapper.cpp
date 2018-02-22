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

#include "local-stream.hpp"
#include "simple-log.hpp"
#include "helpers/face-processor.hpp"
#include "helpers/key-chain-manager.hpp"

using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::helpers;
using namespace boost::chrono;
using namespace ndnlog::new_api;

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

ndnrtc::IStream* ndnrtc_createRemoteStream(RemoteStreamParams params, LibLog loggerSink)
{
	boost::shared_ptr<Logger> callbackLogger = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelDebug,
		boost::make_shared<CallbackSink>(loggerSink));
	callbackLogger->log(ndnlog::NdnLoggerLevelInfo) << "Setting up Remote Video Stream for " 
		<< params.basePrefix << ":" << params.streamName
		<< " (jitter size " << params.jitterSizeMs 
		<< "ms, lifetime " << params.lifetimeMs << "ms)" << std::endl;

	RemoteVideoStream *stream = new RemoteVideoStream(LibFaceProcessor->getIo(),
		LibFaceProcessor->getFace(),
		LibKeyChainManager->instanceKeyChain(),
		std::string(params.basePrefix), std::string(params.streamName),
		params.lifetimeMs, params.jitterSizeMs);
	stream->setLogger(callbackLogger);

	return stream;
}

void ndnrtc_destroyRemoteStream(ndnrtc::IStream* remoteStreamObject)
{
	if (remoteStreamObject)
		delete remoteStreamObject;
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

	{ // registering prefix for serving signing identity: <signing-identity>/KEY
		Name prefix(signingIdentityStr);
		prefix.append("KEY");
		registerPrefix(prefix, logger);
	}

	{ // registering prefix for serving instance identity: <instance-identity>/KEY
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
