//
//  service-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "service-channel.h"
#include "interest-queue.h"
#include "frame-data.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;
using namespace boost;
using namespace ndnlog;

unsigned int ServiceChannel::DefaultUpdateIntervalMs = 1000; // 1 sec

ServiceChannel::ServiceChannel(IServiceChannelPublisherCallback* callback,
                               boost::shared_ptr<FaceProcessor> faceProcessor,
                               unsigned int freshnessIntervalMs):
isMonitoring_(false),
updateCounter_(0),
monitoringThread_(*ThreadWrapper::CreateThread(ServiceChannel::processMonitoring, this)),
monitorTimer_(*EventWrapper::Create()),
callback_(callback),
faceProcessor_(faceProcessor),
interestQueue_(new InterestQueue(faceProcessor->getFaceWrapper())),
sessionInfoFreshnessMs_(freshnessIntervalMs)
{
    // TBD: set observer for interest queue
//    interestQueue_->setObserver(this);
    init();
}

ServiceChannel::ServiceChannel(IServiceChannelListenerCallback* callback,
                               boost::shared_ptr<FaceProcessor> faceProcessor,
                               unsigned int updateIntervalMs):
isMonitoring_(false),
updateCounter_(0),
monitoringThread_(*ThreadWrapper::CreateThread(ServiceChannel::processMonitoring, this)),
monitorTimer_(*EventWrapper::Create()),
callback_(callback),
faceProcessor_(faceProcessor),
interestQueue_(new InterestQueue(faceProcessor->getFaceWrapper())),
updateIntervalMs_(updateIntervalMs)
{
    init();
    interestQueue_->setDescription(std::string("service-iqueue"));
}

void
ServiceChannel::startSessionInfoBroadcast(const std::string& sessionInfoPrefixString,
                                          const boost::shared_ptr<KeyChain> keyChain,
                                          const Name& signingCertificateName)
{
    Name sessionInfoPrefix(sessionInfoPrefixString.c_str());
    
    ndnKeyChain_ = keyChain;
    signingCertificateName_ = signingCertificateName;
    registeredPrefixId_ = faceProcessor_->getFaceWrapper()->registerPrefix(sessionInfoPrefix,
                                                            bind(&ServiceChannel::onInterest, this, _1, _2, _3),
                                                            bind(&ServiceChannel::onRegisterFailed, this, _1));
}

void
ServiceChannel::stopSessionInfoBroadcast()
{
    faceProcessor_->getFaceWrapper()->unregisterPrefix(registeredPrefixId_);
}

void
ServiceChannel::startMonitor(const std::string& sessionInfoPrefix)
{
    sessionInfoPrefix_ = Name(sessionInfoPrefix.c_str());
    startMonitorThread();
}

void
ServiceChannel::stopMonitor()
{
    stopMonitorThread();
}

void
ServiceChannel::setLogger(ndnlog::new_api::Logger* logger)
{
    interestQueue_->setLogger(logger);
    ILoggingObject::setLogger(logger);
}

#pragma mark - private

void
ServiceChannel::init()
{
    description_ = "service-channel";
    memset(&videoParams_, 0, sizeof(ParamsStruct));
    memset(&audioParams_, 0, sizeof(ParamsStruct));
}

void
ServiceChannel::startMonitorThread()
{
    isMonitoring_ = true;
    
    unsigned int tid;
    monitoringThread_.Start(tid);
}

void
ServiceChannel::stopMonitorThread()
{
    isMonitoring_ = false;
    
    monitorTimer_.Set();
    monitoringThread_.Stop();
    monitoringThread_.SetNotAlive();
}

bool
ServiceChannel::monitor()
{
    requestSessionInfo();
    
    monitorTimer_.StartTimer(false, updateIntervalMs_);
    monitorTimer_.Wait(WEBRTC_EVENT_INFINITE);
    
    return isMonitoring_;
}

void
ServiceChannel::requestSessionInfo()
{
    ndn::Interest interest(sessionInfoPrefix_, updateIntervalMs_);
    interest.setMustBeFresh(true);
    
    interestQueue_->enqueueInterest(interest,
                                    Priority::fromAbsolutePriority(1),
                                    bind(&ServiceChannel::onData, this, _1, _2),
                                    bind(&ServiceChannel::onTimeout, this, _1));
}

void
ServiceChannel::onData(const shared_ptr<const Interest>& interest,
                       const shared_ptr<Data>& data)
{
    SessionInfoData sessionInfoData(data->getContent().size(), data->getContent().buf());
    
    if (sessionInfoData.isValid())
    {
        SessionInfo sessionInfo;
        
        sessionInfoData.getSessionInfo(sessionInfo);
        updateParametersFromInfo(sessionInfo);
    }
    else
        getCallbackAsListener()->onUpdateFailedWithError("got malformed session info data");
}

void
ServiceChannel::onTimeout(const shared_ptr<const Interest>& interest)
{
    stopMonitorThread();
    getCallbackAsListener()->onUpdateFailedWithTimeout();
}

void
ServiceChannel::updateParametersFromInfo(const SessionInfo &sessionInfo)
{
#warning TBD!
//    ParamsStruct videoParams, audioParams;
//    memset(&videoParams, 0, sizeof(ParamsStruct));
//    memset(&audioParams, 0, sizeof(ParamsStruct));
//    
//    if (RESULT_NOT_FAIL(sessionInfo.getParams(videoParams, audioParams)))
//    {
//        bool needUpdate = false;
//        
//        if ((needUpdate = hasUpdates(videoParams_, videoParams)))
//            SessionInfo::updateParams(videoParams_, videoParams);
//        
//        if ((needUpdate |= hasUpdates(audioParams_, audioParams)))
//            SessionInfo::updateParams(audioParams_, audioParams);
//        
//        if (needUpdate)
//        {
//            getCallbackAsListener()->onProducerParametersUpdated(videoParams_, audioParams_);
//        }
//    }
//    else
//        getCallbackAsListener()->onUpdateFailedWithError("got malformed session info data");
    
    updateCounter_++;
}

bool
ServiceChannel::hasUpdates(const ParamsStruct& oldParams,
                           const ParamsStruct& newParams)
{
    bool hasUpdated = false;
    
    hasUpdated = (oldParams.nStreams != newParams.nStreams);
    hasUpdated |= (oldParams.segmentSize != newParams.segmentSize);
    
    if (!hasUpdated)
    {
        for (int i = 0; i < oldParams.nStreams && !hasUpdated; i++)
        {
            hasUpdated = (oldParams.streamsParams[i].gop != newParams.streamsParams[i].gop);
            hasUpdated |= (oldParams.streamsParams[i].codecFrameRate != newParams.streamsParams[i].codecFrameRate);
            hasUpdated |= (oldParams.streamsParams[i].startBitrate != newParams.streamsParams[i].startBitrate);
            hasUpdated |= (oldParams.streamsParams[i].encodeWidth != newParams.streamsParams[i].encodeWidth);
            hasUpdated |= (oldParams.streamsParams[i].encodeHeight != newParams.streamsParams[i].encodeHeight);
        }
    }
    
    return hasUpdated;
}

void
ServiceChannel::onInterest(const shared_ptr<const Name>& prefix,
                           const shared_ptr<const Interest>& interest,
                           ndn::Transport& transport)
{
    if (callback_)
    {
        shared_ptr<SessionInfo> sessionInfo = getCallbackAsPublisher()->onPublishSessionInfo();
        SessionInfoData sessionInfoData(*sessionInfo);
        
        Data ndnData(*prefix);
        ndnData.getMetaInfo().setFreshnessPeriod(sessionInfoFreshnessMs_);
        ndnData.setContent(sessionInfoData.getData(), sessionInfoData.getLength());
        ndnKeyChain_->sign(ndnData, signingCertificateName_);
        
        SignedBlob encodedData = ndnData.wireEncode();
        
        faceProcessor_->getFaceWrapper()->synchronizeStart();
        faceProcessor_->getTransport()->send(*encodedData);
        faceProcessor_->getFaceWrapper()->synchronizeStop();
    }
    else
        LogWarnC << "got session info interest,"
        << " but no dispatching callback provided" << std::endl;
}

void
ServiceChannel::onRegisterFailed(const shared_ptr<const Name>&
                                   prefix)
{
    LogErrorC << "registration failed " << prefix << std::endl;

    if (callback_)
        getCallbackAsPublisher()->onSessionInfoBroadcastFailed();
}
