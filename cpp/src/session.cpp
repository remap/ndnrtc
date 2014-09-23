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
userPrefix_(*NdnRtcNamespace::getProducerPrefix(generalParams.prefix_, username_)),
sessionObserver_(NULL),
status_(SessionOffline)
{
    description_ = "session";
    this->setLogger(new Logger(generalParams.loggingLevel_,
                               NdnRtcUtils::toString("producer-%s.log", username.c_str())));

    userKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix_);
    
    mainFaceProcessor_ = FaceProcessor::createFaceProcessor(generalParams_.host_, generalParams_.portNum_, NdnRtcNamespace::defaultKeyChain());
    sessionCache_.reset(new MemoryContentCache(mainFaceProcessor_->getFaceWrapper()->getFace().get()));
}

Session::~Session()
{
    stop();
}


int
Session::start()
{
    mainFaceProcessor_->startProcessing();
    startServiceChannel();
    LogInfoC << "session started" << std::endl;
    
    return RESULT_OK;
}

int
Session::stop()
{
    mainFaceProcessor_->stopProcessing();
    
    int res = serviceChannel_->stopSessionInfoBroadcast();
    
    if (RESULT_NOT_FAIL(res))
    {
        LogInfoC << "session stopped" << std::endl;
        switchStatus(SessionOffline);
    }
    
    return RESULT_OK;
}

int
Session::addLocalStream(const MediaStreamParams& params,
                        IExternalCapturer** const capturer,
                        std::string& streamPrefix)
{
    MediaStreamSettings mediaStreamSettings(params);
    
    mediaStreamSettings.useFec_ = generalParams_.useFec_;
    mediaStreamSettings.userPrefix_ = getPrefix();
    mediaStreamSettings.keyChain_ = userKeyChain_;
    mediaStreamSettings.faceProcessor_ = mainFaceProcessor_;
    mediaStreamSettings.memoryCache_ = sessionCache_;
    
    shared_ptr<MediaStream> mediaStream;
    StreamMap& streamMap = (params.type_ == MediaStreamParams::MediaStreamTypeAudio)?audioStreams_:videoStreams_;
    
    if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
        mediaStream.reset(new AudioStream());
    else
        mediaStream.reset(new VideoStream());
    
    mediaStream->registerCallback(this);
    mediaStream->setLogger(logger_);
    
    if (RESULT_FAIL(mediaStream->init(mediaStreamSettings)))
        return notifyError(-1, "couldn't initialize media stream");
    
    streamMap[mediaStream->getPrefix()] = mediaStream;
    streamPrefix = mediaStream->getPrefix();
    
    if (params.type_ == MediaStreamParams::MediaStreamTypeVideo)
        *capturer = dynamic_pointer_cast<VideoStream>(mediaStream)->getCapturer();
    
    switchStatus(SessionOnlinePublishing);
    
    return RESULT_OK;
}

int
Session::removeLocalStream(const std::string& streamPrefix)
{
    StreamMap::iterator it = audioStreams_.find(streamPrefix);
    StreamMap* streamMap = &audioStreams_;
    
    if (it == audioStreams_.end())
    {
        it = videoStreams_.find(streamPrefix);
        
        if (it == videoStreams_.end())
            return RESULT_ERR;
        
        streamMap = &videoStreams_;
    }
    
    it->second->release();
    streamMap->erase(it);
    
    if (audioStreams_.size() == 0 && videoStreams_.size() == 0)
        switchStatus(SessionOnlineNotPublishing);
    
    return RESULT_OK;
}

// private
void
Session::switchStatus(SessionStatus status)
{
    if (status != status_)
    {
        status_ = status;
        
        if (sessionObserver_)
            sessionObserver_->onSessionStatusUpdate(username_.c_str(),
                                                    userPrefix_.c_str(),
                                                    status_);
    }
}

void
Session::startServiceChannel()
{
    serviceChannel_.reset(new ServiceChannel(this, mainFaceProcessor_));
    serviceChannel_->registerCallback(this);
    
    if (RESULT_NOT_FAIL(serviceChannel_->startSessionInfoBroadcast(*NdnRtcNamespace::getSessionInfoPrefix(userPrefix_),
                                               userKeyChain_,
                                               *NdnRtcNamespace::certificateNameForUser(userPrefix_))))
    {
        switchStatus(SessionOnlineNotPublishing);
        
    }
}

// IServiceChannelPublisherCallback
void
Session::onSessionInfoBroadcastFailed()
{
    LogErrorC << "failed to register prefix for session info" << std::endl;
    switchStatus(SessionOffline);
}

boost::shared_ptr<SessionInfo>
Session::onPublishSessionInfo()
{
    LogInfoC << "session info requested" << std::endl;

    shared_ptr<SessionInfo> sessionInfo(new SessionInfo());

    for (StreamMap::iterator it = audioStreams_.begin(); it != audioStreams_.end(); it++)
    {
        MediaStreamParams* streamParams = new MediaStreamParams(it->second->getSettings().streamParams_);
        
        sessionInfo->audioStreams_.push_back(streamParams);
    }
    
    for (StreamMap::iterator it = videoStreams_.begin(); it != videoStreams_.end(); it++)
    {
        MediaStreamParams* streamParams = new MediaStreamParams(it->second->getSettings().streamParams_);
        
        sessionInfo->videoStreams_.push_back(streamParams);
    }
    
    return sessionInfo;
}

void
Session::onError(const char *errorMessage)
{
    std::string extendedMessage = NdnRtcUtils::toString("[session %s] %s",
                                                        getPrefix().c_str(),
                                                        errorMessage);
    NdnRtcComponent::onError(extendedMessage.c_str());
    
    if (sessionObserver_)
        sessionObserver_->onSessionError(username_.c_str(), userPrefix_.c_str(),
                                         status_, -1, errorMessage);
}
