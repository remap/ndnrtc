//
//  session.cpp
//  libndnrtc
//
//  Created by Peter Gusev on 9/18/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#include "session.h"
#include "ndnrtc-namespace.h"

using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace boost;

Session::Session(const std::string username,
                 const GeneralParams& generalParams):
NdnRtcComponent(),
username_(username),
generalParams_(generalParams),
userPrefix_(*NdnRtcNamespace::getProducerPrefix(generalParams_.prefix_, username_))
{
    description_ = "session";
    this->setLogger(new Logger(generalParams.loggingLevel_,
                               NdnRtcUtils::toString("producer-%s.log", username.c_str())));

    userKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix_);
    
    mainFaceProcessor_ = FaceProcessor::createFaceProcessor(generalParams_.host_, generalParams_.portNum_, NdnRtcNamespace::defaultKeyChain());
}

Session::~Session()
{
    
}


int
Session::start()
{
    startServiceChannel();
    LogInfoC << "session started" << std::endl;
    
    return RESULT_OK;
}

int
Session::stop()
{
    serviceChannel_->stopSessionInfoBroadcast();
    LogInfoC << "session stopped" << std::endl;
    
    return RESULT_OK;
}

// private
void
Session::startServiceChannel()
{
    serviceChannel_.reset(new ServiceChannel(this, mainFaceProcessor_));
    serviceChannel_->startSessionInfoBroadcast(*NdnRtcNamespace::getSessionInfoPrefix(userPrefix_),
                                               userKeyChain_,
                                               *NdnRtcNamespace::certificateNameForUser(userPrefix_));
}

// IServiceChannelPublisherCallback
void
Session::onSessionInfoBroadcastFailed()
{
    LogErrorC << "failed to register prefix for session info" << std::endl;
}

boost::shared_ptr<SessionInfo>
Session::onPublishSessionInfo()
{
//    LogInfoC << "session info requested" << std::endl;
//    
    shared_ptr<SessionInfo> sessionInfo(new SessionInfo());
//
//    for (StreamMap::iterator it = audioStreams_.begin(); it != audioStreams_.end(); it++)
//    {
//        
//        MediaStreamParams *mediaStreamParams(it->second->getSettings()->getTranslatedToParams());
//        sessionInfo->
//    }
    
    return sessionInfo;
}
