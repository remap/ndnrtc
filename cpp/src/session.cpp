//
//  session.cpp
//  libndnrtc
//
//  Created by Peter Gusev on 9/18/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#include <boost/thread/lock_guard.hpp>

#include "session.h"
#include "ndnrtc-namespace.h"
#include "ndnrtc-utils.h"
#include "error-codes.h"

using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace boost;

Session::Session():
NdnRtcComponent(),
sessionObserver_(NULL),
status_(SessionOffline)
{
    description_ = "session";
}

Session::~Session()
{
    std::cout << " session dtor begin" << std::endl;
    stop();
}

int
Session::init(const std::string username,
              const GeneralParams& generalParams,
              boost::shared_ptr<FaceProcessor> mainFaceProcessor)
{
    username_ = username;
    generalParams_ = generalParams;
    userPrefix_ = *NdnRtcNamespace::getProducerPrefix(generalParams.prefix_, username_);
    
    std::string logFileName = NdnRtcUtils::toString("producer-%s.log", username.c_str());
    this->setLogger(new Logger(generalParams.loggingLevel_,
                               NdnRtcUtils::getFullLogPath(generalParams, logFileName)));
    isLoggerCreated_ = true;
    
    userKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix_);

    try {
        mainFaceProcessor_ = mainFaceProcessor;
    }
    catch (std::exception& exception) {
        notifyError(NRTC_ERR_LIBERROR, "Exception from NDN-CPP library: %s\n"
                    "Make sure your NDN daemon is running", exception.what());
        
        return RESULT_ERR;
    }

    sessionCache_.reset(new MemoryContentCache(mainFaceProcessor_->getFaceWrapper()->getFace().get()));
    
    return RESULT_OK;
}

int
Session::start()
{
    switchStatus(SessionOnlineNotPublishing);
    
    scheduleJob(500000, boost::bind(&Session::updateSessionInfo, this));
    
    LogInfoC << "session started" << std::endl;
    
    return RESULT_OK;
}

int
Session::stop()
{
    int res = RESULT_OK;
    
    NdnRtcUtils::performOnBackgroundThread([this]()->void{
        for (auto audioStream:audioStreams_)
            audioStream.second->release();
        for (auto videoStream:videoStreams_)
            videoStream.second->release();
        
        audioStreams_.clear();
        videoStreams_.clear();
        
        if (sessionCache_.get())
            sessionCache_->unregisterAll();
    });
    
    if (RESULT_NOT_FAIL(res))
    {
        LogInfoC << "session stopped" << std::endl;
        switchStatus(SessionOffline);
        logger_->flush();
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

    // here we have a choice - use session-level memory cache or create a new
    // one specifically for the stream
    // as MemoryContentCache does not support unregistering individual prefixes,
    // a stream will not be able to "clean" after itself upon removal
    // that's why a new stream-level content cache is used and it can be purged
    // by stream painlessly
    mediaStreamSettings.memoryCache_.reset(new MemoryContentCache(mainFaceProcessor_->getFaceWrapper()->getFace().get()));
    
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
    
    if (sessionObserver_)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(observerMutex_);
        sessionObserver_->onSessionInfoUpdate(*this->getSessionInfo());
    }
    
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

    if (sessionObserver_)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(observerMutex_);
        sessionObserver_->onSessionInfoUpdate(*this->getSessionInfo());
    }

    
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
        {
            boost::lock_guard<boost::recursive_mutex> scopedLock(observerMutex_);
            sessionObserver_->onSessionStatusUpdate(username_.c_str(),
                                                    userPrefix_.c_str(),
                                                    status_);
        }
    }
}

bool
Session::updateSessionInfo()
{
    if (sessionObserver_)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(observerMutex_);
        sessionObserver_->onSessionInfoUpdate(*this->getSessionInfo());
    }
    
    return true;
}

boost::shared_ptr<SessionInfo>
Session::getSessionInfo()
{
    LogDebugC << "session info requested" << std::endl;

    shared_ptr<SessionInfo> sessionInfo(new SessionInfo());
    sessionInfo->sessionPrefix_ = userPrefix_;

    for (StreamMap::iterator it = audioStreams_.begin(); it != audioStreams_.end(); it++)
    {
        MediaStreamParams* streamParams = new MediaStreamParams(it->second->getStreamParameters());
        
        sessionInfo->audioStreams_.push_back(streamParams);
    }
    
    for (StreamMap::iterator it = videoStreams_.begin(); it != videoStreams_.end(); it++)
    {
        MediaStreamParams* streamParams = new MediaStreamParams(it->second->getStreamParameters());
        
        sessionInfo->videoStreams_.push_back(streamParams);
    }
    
    return sessionInfo;
}

void
Session::onError(const char *errorMessage, const int errorCode)
{
    std::string extendedMessage = NdnRtcUtils::toString("[session %s] %s",
                                                        getPrefix().c_str(),
                                                        errorMessage);
    
    if (sessionObserver_)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(observerMutex_);
        sessionObserver_->onSessionError(username_.c_str(), userPrefix_.c_str(),
                                         status_, errorCode, errorMessage);
    }
    else
        NdnRtcComponent::onError(extendedMessage.c_str(), errorCode);
}
