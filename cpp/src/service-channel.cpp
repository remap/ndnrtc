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
#include "ndnrtc-namespace.h"

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
monitoringThread_(*ThreadWrapper::CreateThread(ServiceChannel::processMonitoring, this, "service-channel")),
monitorTimer_(*EventTimerWrapper::Create()),
serviceChannelCallback_(callback),
faceProcessor_(faceProcessor),
sessionInfoFreshnessMs_(freshnessIntervalMs)
{
    init();
}

ServiceChannel::ServiceChannel(IServiceChannelListenerCallback* callback,
                               boost::shared_ptr<FaceProcessor> faceProcessor,
                               unsigned int updateIntervalMs):
isMonitoring_(false),
updateCounter_(0),
monitoringThread_(*ThreadWrapper::CreateThread(ServiceChannel::processMonitoring, this, "service-channel")),
monitorTimer_(*EventTimerWrapper::Create()),
serviceChannelCallback_(callback),
faceProcessor_(faceProcessor),
updateIntervalMs_(updateIntervalMs)
{
    init();
}

int
ServiceChannel::startSessionInfoBroadcast(const std::string& sessionInfoPrefixString,
                                          const boost::shared_ptr<KeyChain> keyChain,
                                          const Name& signingCertificateName)
{
    int res = RESULT_ERR;
    Name sessionInfoPrefix(sessionInfoPrefixString.c_str());
    
    ndnKeyChain_ = keyChain;
    signingCertificateName_ = signingCertificateName;
    
    try {
        registeredPrefixId_ = faceProcessor_->getFaceWrapper()->registerPrefix(sessionInfoPrefix,
                                                                               bind(&ServiceChannel::onInterest, this, _1, _2, _3),
                                                                               bind(&ServiceChannel::onRegisterFailed, this, _1));
        res = RESULT_OK;
    }
    catch (std::exception &e)
    {
        notifyError(NRTC_ERR_LIBERROR, "Exception from NDN library: %s\n"
                    "Make sure your local NDN daemon is configured and running",
                    e.what());
        
        if (serviceChannelCallback_)
            getCallbackAsPublisher()->onSessionInfoBroadcastFailed();
    }
    
    return res;
}

int
ServiceChannel::stopSessionInfoBroadcast()
{
    int res = RESULT_ERR;
    
    try {
        faceProcessor_->getFaceWrapper()->unregisterPrefix(registeredPrefixId_);
        res = RESULT_OK;
    }
    catch (std::exception &e) {
        notifyError(NRTC_ERR_LIBERROR, "Exception from NDN library: %s"
                    "Make sure your local NDN daemon is configured and running"
                    , e.what());
    }
    
    return res;
}

void
ServiceChannel::startMonitor(const std::string& sessionPrefix)
{
    sessionInfoPrefix_ = Name(*NdnRtcNamespace::getSessionInfoPrefix(sessionPrefix));
    startMonitorThread();
}

void
ServiceChannel::stopMonitor()
{
    stopMonitorThread();
}

#pragma mark - private

void
ServiceChannel::init()
{
    description_ = "service-channel";
}

void
ServiceChannel::startMonitorThread()
{
    isMonitoring_ = true;
    monitoringThread_.Start();
}

void
ServiceChannel::stopMonitorThread()
{
    isMonitoring_ = false;
    
    faceProcessor_->getFaceWrapper()->removePendingInterest(pendingInterestId_);
    pendingInterestId_ = 0;
    
    monitorTimer_.Set();
    monitoringThread_.Stop();
//    monitoringThread_.SetNotAlive();
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
 
    try {
        pendingInterestId_ = faceProcessor_->getFaceWrapper()->expressInterest(interest,
                                                                               bind(&ServiceChannel::onData, this, _1, _2),
                                                                               bind(&ServiceChannel::onTimeout, this, _1));
    }
    catch (std::exception &exception) {
        if (serviceChannelCallback_)
            getCallbackAsListener()->onUpdateFailedWithError(NRTC_ERR_LIBERROR,
                                                             exception.what());
            
        notifyError(NRTC_ERR_LIBERROR, "Exception from NDN-CPP library: %s\n"
                    "Make sure your local NDN daemon is configured and running",
                    exception.what());
    }
}

void
ServiceChannel::onData(const shared_ptr<const Interest>& interest,
                       const shared_ptr<Data>& data)
{
    pendingInterestId_ = 0;
    
    if (serviceChannelCallback_)
    {
        SessionInfoData sessionInfoData(data->getContent().size(), data->getContent().buf());
        
        if (sessionInfoData.isValid())
        {
            SessionInfo sessionInfo;
            
            sessionInfoData.getSessionInfo(sessionInfo);
            getCallbackAsListener()->onSessionInfoUpdate(sessionInfo);
            updateCounter_++;
        }
        else
            getCallbackAsListener()->onUpdateFailedWithError(NRTC_ERR_MALFORMED,
                                                             "got malformed session info data");
    }
}

void
ServiceChannel::onTimeout(const shared_ptr<const Interest>& interest)
{
    if (serviceChannelCallback_)
        getCallbackAsListener()->onUpdateFailedWithTimeout();
}

void
ServiceChannel::onInterest(const shared_ptr<const Name>& prefix,
                           const shared_ptr<const Interest>& interest,
                           ndn::Transport& transport)
{
    if (serviceChannelCallback_)
    {
        sessionInfo_ = getCallbackAsPublisher()->onPublishSessionInfo();
        SessionInfoData sessionInfoData(*sessionInfo_);
        
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

    if (serviceChannelCallback_)
        getCallbackAsPublisher()->onSessionInfoBroadcastFailed();
}
