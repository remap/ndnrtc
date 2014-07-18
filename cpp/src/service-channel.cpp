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
using namespace std;

unsigned int ServiceChannel::DefaultUpdateIntervalMs = 1000; // 1 sec

ServiceChannel::ServiceChannel(IServiceChannelCallback* callback,
                               shared_ptr<InterestQueue> interestQueue,
                               unsigned int updateIntervalMs):
isMonitoring_(false),
updateCounter_(0),
monitoringThread_(*ThreadWrapper::CreateThread(ServiceChannel::processMonitoring, this)),
monitorTimer_(*EventWrapper::Create()),
callback_(callback),
interestQueue_(interestQueue),
updateIntervalMs_(updateIntervalMs)
{
    description_ = "service-channel";
    memset(&videoParams_, 0, sizeof(ParamsStruct));
    memset(&audioParams_, 0, sizeof(ParamsStruct));
}


void
ServiceChannel::startMonitor(const std::string& sessionInfoPrefix)
{
    sessionInfoPrefix_ = Name(sessionInfoPrefix.c_str());
    requestSessionInfo();
}

void
ServiceChannel::stopMonitor()
{
    stopMonitorThread();
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
    monitorTimer_.StartTimer(false, updateIntervalMs_);
    monitorTimer_.Wait(WEBRTC_EVENT_INFINITE);
    
    updateCounter_++;
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
    SessionInfo sessionInfo(data->getContent().size(), data->getContent().buf());
    
    if (sessionInfo.isValid())
        updateParametersFromInfo(sessionInfo);
    else
        callback_->onUpdateFailedWithError("got malformed session info data");
}

void
ServiceChannel::onTimeout(const shared_ptr<const Interest>& interest)
{
    stopMonitorThread();
    callback_->onUpdateFailedWithTimeout();
}

void
ServiceChannel::updateParametersFromInfo(const SessionInfo &sessionInfo)
{
    ParamsStruct videoParams, audioParams;
    memset(&videoParams, 0, sizeof(ParamsStruct));
    memset(&audioParams, 0, sizeof(ParamsStruct));
    
    if (RESULT_NOT_FAIL(sessionInfo.getParams(videoParams, audioParams)))
    {
        bool needUpdate = false;
        
        if ((needUpdate = hasUpdates(videoParams_, videoParams)))
            SessionInfo::updateParams(videoParams_, videoParams);
        
        if ((needUpdate |= hasUpdates(audioParams_, audioParams)))
            SessionInfo::updateParams(audioParams_, audioParams);
        
        if (needUpdate)
        {
            callback_->onProducerParametersUpdated(videoParams_, audioParams_);
            
            if (updateCounter_ == 0)
                startMonitorThread();
        }
    }
    else
        callback_->onUpdateFailedWithError("got malformed session info data");
    
    updateCounter_++;
}



