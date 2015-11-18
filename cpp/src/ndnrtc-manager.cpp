// 
//  ndnrtc-manager.cpp
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <memory>
#include <signal.h>
#include <boost/thread/mutex.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include "ndnrtc-manager.h"
#include "ndnrtc-namespace.h"
#include "audio-consumer.h"
#include "video-consumer.h"
#include "external-capturer.hpp"
#include "session.h"
#include "error-codes.h"
#include "frame-data.h"
#include "ndnrtc-utils.h"
#include "params-validator.h"

using namespace boost;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

typedef std::map<std::string, shared_ptr<Consumer>> ConsumerStreamMap;

static shared_ptr<Session> ActiveSession;
static ConsumerStreamMap ActiveStreamConsumers;
static boost::asio::io_service libIoService;
static boost::asio::steady_timer recoveryCheckTimer(libIoService);

void init();
void cleanup();
void failure();
void reset();
void recoveryCheck(const boost::system::error_code& e);

//******************************************************************************
class NdnRtcLibraryInternalObserver :   public INdnRtcComponentCallback,
public INdnRtcObjectObserver,
public INdnRtcLibraryObserver
{
public:
    NdnRtcLibraryInternalObserver():libObserver_(NULL){}
    ~NdnRtcLibraryInternalObserver(){}
    
    void
    errorWithCode(int errCode, const char* message)
    {
        if (libObserver_)
            libObserver_->onErrorOccurred(errCode, message);
    }
    
    void
    setLibraryObserver(INdnRtcLibraryObserver* libObserver)
    {
        libObserver_ = libObserver;
    }
    
    // INdnRtcLibraryObserver
    void onStateChanged(const char *state, const char *args)
    {
        if (libObserver_)
            libObserver_->onStateChanged(state, args);
    }
    
    void onErrorOccurred(int errorCode, const char *errorMessage)
    {
        if (libObserver_)
            libObserver_->onErrorOccurred(errorCode, errorMessage);
    }
    
    // INdnRtcObjectObserver
    void onErrorOccurred(const char *errorMessage)
    {
        if (libObserver_)
            libObserver_->onErrorOccurred(-1, errorMessage);
    }
    
    // INdnRtcComponentCallback
    void onError(const char *errorMessage,
                 const int errCode)
    {
        if (libObserver_)
            libObserver_->onErrorOccurred(errCode, errorMessage);
    }
    
private:
    INdnRtcLibraryObserver* libObserver_;
};

static NdnRtcLibraryInternalObserver LibraryInternalObserver;
//******************************************************************************

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
    static int initialized = 0;
    if (!initialized)
    {
        initialized = 1;
        NdnRtcUtils::setIoService(libIoService);
        
    }
}

__attribute__((destructor))
static void destructor(){
}

//******************************************************************************
static void signalHandler(int signal, siginfo_t *siginfo, void *context)
{
    LogError(LIB_LOG) << "Received signal " << signal << std::endl;
    
//    if (signal == SIGPIPE)
//    {
//        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_SIGPIPE,
//                                                "Make sure your NDN daemon is running");
//    }
}

//******************************************************************************
#pragma mark - construction/destruction
NdnRtcManager::NdnRtcManager()
{
    init();
    initialized_ = true;
    failed_ = false;
}

NdnRtcManager::~NdnRtcManager()
{
    cleanup();
    initialized_ = false;
}

//******************************************************************************
#pragma mark - public
NdnRtcManager& NdnRtcManager::getSharedInstance()
{
    static NdnRtcManager NdnRtcLib;
    return NdnRtcLib;
}

void NdnRtcManager::setObserver(INdnRtcLibraryObserver *observer)
{
    LogInfo(LIB_LOG) << "Set library observer " << observer << std::endl;
    
    LibraryInternalObserver.setLibraryObserver(observer);
}

//******************************************************************************
std::string NdnRtcManager::startSession(const std::string& username,
                                        const new_api::GeneralParams& generalParams,
                                        ISessionObserver *sessionObserver)
{
    std::string sessionPrefix = "";
    
    try {
        ParamsValidator::validateName(username, ParamsValidator::MaxUserNameLen);
        ParamsValidator::validateParams(generalParams);
        
        Logger::getLogger(LIB_LOG).flush();
        LIB_LOG = NdnRtcUtils::getFullLogPath(generalParams, generalParams.logFile_);
        Logger::getLogger(LIB_LOG).setLogLevel(generalParams.loggingLevel_);
        
        LogInfo(LIB_LOG) << "Starting session for user " << username << "..." << std::endl;
        
        NdnRtcUtils::createLibFace(generalParams);
        
        sessionPrefix = *NdnRtcNamespace::getProducerPrefix(generalParams.prefix_, username);
        
        LogInfo(LIB_LOG) << "Creating new session instance..." << std::endl;
        
        ActiveSession.reset(new Session());
        ActiveSession->setSessionObserver(sessionObserver);
        ActiveSession->registerCallback(&LibraryInternalObserver);
        
        if (RESULT_NOT_FAIL(ActiveSession->init(username, generalParams, NdnRtcUtils::getLibFace())))
        {
            ActiveSession->start();
            sessionPrefix = ActiveSession->getPrefix();
        }
        else
        {
            LogError(LIB_LOG) << "Session initialization failed" << std::endl;
            sessionPrefix = "";
        }
        
        LogInfo(LIB_LOG) << "Session started (prefix " << sessionPrefix
        << ")" << std::endl;
    }
    catch (param_validation_exception &e) {
        LogError(LIB_LOG) << "parameters validation failed: " << e.what() << std::endl;
        LogError(LIB_LOG) << boost::diagnostic_information(e) << std::endl;
        
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_VALIDATION, e.what());
    }
    
    return sessionPrefix;
}

int NdnRtcManager::stopSession(const std::string& userPrefix)
{
    int res = RESULT_ERR;
    
    LogInfo(LIB_LOG) << "Stopping session for prefix " << userPrefix << std::endl;
    
    if (userPrefix == ActiveSession->getPrefix())
    {
        res = ActiveSession->stop();
        
        LogInfo(LIB_LOG) << "Session was successfully stopped" << std::endl;
    }
    else
        LogError(LIB_LOG) << "Session was not found" << std::endl;
    
    return res;
}

std::string NdnRtcManager::addLocalStream(const std::string& sessionPrefix,
                                          const new_api::MediaStreamParams& params,
                                          IExternalCapturer** const capturer)
{
    std::string streamPrefix = "";
    
    LogInfo(LIB_LOG) << "Adding new stream for session "
    << sessionPrefix << "..." << std::endl;
    
    if (sessionPrefix != ActiveSession->getPrefix())
    {
        LogError(LIB_LOG) << "Session was not found" << std::endl;
    }
    else
    {
        if (RESULT_NOT_FAIL(ActiveSession->addLocalStream(params, capturer, streamPrefix)))
        {
            LogInfo(LIB_LOG) << "successfully added stream with prefix "
            << streamPrefix << std::endl;
        }
        else
            LogError(LIB_LOG) << "Failed to add new stream" << std::endl;
    }
    
    
    return streamPrefix;
}

int NdnRtcManager::removeLocalStream(const std::string& sessionPrefix,
                                     const std::string& streamPrefix)
{
    int res = RESULT_ERR;
    
    LogInfo(LIB_LOG) << "Removing stream " << streamPrefix
    << " from session " << sessionPrefix << "..." << std::endl;
    
    if (sessionPrefix != ActiveSession->getPrefix())
    {
        LogError(LIB_LOG) << "Session was not found" << std::endl;
    }
    else
    {
        ActiveSession->removeLocalStream(streamPrefix);
        LogInfo(LIB_LOG) << "Stream was removed successfully" << std::endl;
        res = RESULT_OK;
    }
    
    return res;
}

std::string
NdnRtcManager::addRemoteStream(const std::string& remoteSessionPrefix,
                               const std::string& threadName,
                               const new_api::MediaStreamParams& params,
                               const new_api::GeneralParams& generalParams,
                               const new_api::GeneralConsumerParams& consumerParams,
                               IExternalRenderer* const renderer)
{
    std::string streamPrefix = "";
    
    NdnRtcUtils::performOnBackgroundThread([=, &streamPrefix]()->void{
        Logger::getLogger(LIB_LOG).flush();
        LIB_LOG = NdnRtcUtils::getFullLogPath(generalParams, generalParams.logFile_);
        NdnRtcUtils::createLibFace(generalParams);
        
        LogInfo(LIB_LOG) << "Adding remote stream for session "
        << remoteSessionPrefix << "..." << std::endl;
        
        streamPrefix = *NdnRtcNamespace::getStreamPrefix(remoteSessionPrefix, params.streamName_);
        shared_ptr<Consumer> remoteStreamConsumer;
        ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
        
        if (it != ActiveStreamConsumers.end() &&
            it->second->getIsConsuming())
        {
            LogWarn(LIB_LOG) << "Stream was already added" << std::endl;
            
            LibraryInternalObserver.onErrorOccurred(NRTC_ERR_ALREADY_EXISTS,
                                                    "stream is already running");
            streamPrefix = "";
        }
        else
        {
            if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
                remoteStreamConsumer.reset(new AudioConsumer(generalParams, consumerParams));
            else
                remoteStreamConsumer.reset(new VideoConsumer(generalParams, consumerParams, renderer));
            
            if (it != ActiveStreamConsumers.end())
                it->second = remoteStreamConsumer;
            
            ConsumerSettings settings;
            settings.userPrefix_ = remoteSessionPrefix;
            settings.streamParams_ = params;
            settings.faceProcessor_ = NdnRtcUtils::getLibFace();
            
            remoteStreamConsumer->registerCallback(&LibraryInternalObserver);
            
            if (RESULT_FAIL(remoteStreamConsumer->init(settings, threadName)))
            {
                LogError(LIB_LOG) << "Failed to initialize fetching from stream" << std::endl;
                streamPrefix = "";
            }
            else
            {
                std::string username = NdnRtcNamespace::getUserName(remoteSessionPrefix);
                std::string logFile = NdnRtcUtils::getFullLogPath(generalParams,
                                                                  NdnRtcUtils::toString("consumer-%s-%s.log",
                                                                                        username.c_str(),
                                                                                        params.streamName_.c_str()));
                
                remoteStreamConsumer->setLogger(new Logger(generalParams.loggingLevel_,
                                                           logFile));
                
                if (RESULT_FAIL(remoteStreamConsumer->start()))
                    streamPrefix = "";
                else
                {
                    if (it == ActiveStreamConsumers.end())
                        ActiveStreamConsumers[remoteStreamConsumer->getPrefix()] = remoteStreamConsumer;
                    streamPrefix = remoteStreamConsumer->getPrefix();
                }
            }
        }
    });
    
    return streamPrefix;
}

std::string
NdnRtcManager::removeRemoteStream(const std::string& streamPrefix)
{
    std::string logFileName = "";
    
    LogInfo(LIB_LOG) << "Removing stream " << streamPrefix << "..." << std::endl;
    
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LogError(LIB_LOG) << "Stream was not added previously" << std::endl;
    }
    else
    {
        logFileName = it->second->getLogger()->getFileName();
        it->second->stop();
        {
            NdnRtcUtils::performOnBackgroundThread([&]()->void{
                ActiveStreamConsumers.erase(it);
            });
        }
        
        LogInfo(LIB_LOG) << "Stream removed successfully" << std::endl;
    }
    
    return logFileName;
}

int
NdnRtcManager::setStreamObserver(const std::string& streamPrefix,
                                 IConsumerObserver* const observer)
{
    int res = RESULT_ERR;
    
    NdnRtcUtils::performOnBackgroundThread([=, &res]()->void{
        LogInfo(LIB_LOG) << "Setting stream observer " << observer
        << " for stream " << streamPrefix << "..." << std::endl;
        
        ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
        
        if (it == ActiveStreamConsumers.end())
        {
            LogError(LIB_LOG) << "Stream was not added previously" << std::endl;
        }
        else
        {
            res = RESULT_OK;
            it->second->registerObserver(observer);
            LogInfo(LIB_LOG) << "Added observer successfully" << std::endl;
        }
    });
    
    return res;
}

int
NdnRtcManager::removeStreamObserver(const std::string& streamPrefix)
{
    int res = RESULT_ERR;
    
    NdnRtcUtils::performOnBackgroundThread([=, &res]()->void{
        LogInfo(LIB_LOG) << "Removing stream observer for prefix " << streamPrefix
        << "..." << std::endl;
        
        ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
        
        if (it == ActiveStreamConsumers.end())
        {
            LogError(LIB_LOG) << "Couldn't find requested stream" << std::endl;
        }
        else
        {
            res = RESULT_OK;
            it->second->unregisterObserver();
            LogInfo(LIB_LOG) << "Stream observer was removed successfully" << std::endl;
        }
    });
    
    return res;
}

std::string
NdnRtcManager::getStreamThread(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        return "";
    }
    
    return it->second->getCurrentThreadName();
}

int
NdnRtcManager::switchThread(const std::string& streamPrefix,
                            const std::string& threadName)
{
    int res = RESULT_ERR;
    
    NdnRtcUtils::performOnBackgroundThread([=, &res]()->void{
        LogInfo(LIB_LOG) << "Switching to thread " << threadName
        << " for stream " << streamPrefix << std::endl;
        
        ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
        
        if (it == ActiveStreamConsumers.end())
        {
            LogError(LIB_LOG) << "Couldn't find requested stream" << std::endl;
        }
        else
        {
            res = RESULT_OK;
            it->second->switchThread(threadName);
            LogInfo(LIB_LOG) << "Thread switched successfully" << std::endl;
        }
    });
    
    return RESULT_OK;
}

statistics::StatisticsStorage
NdnRtcManager::getRemoteStreamStatistics(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        return *statistics::StatisticsStorage::createConsumerStatistics();
    }
    
    return it->second->getStatistics();;
}

//******************************************************************************
void
NdnRtcManager::getVersionString(char **versionString)
{
    if (versionString)
        memcpy((void*)(*versionString), PACKAGE_VERSION, strlen(PACKAGE_VERSION));
    
    return;
}

void
NdnRtcManager::serializeSessionInfo(const SessionInfo &sessionInfo,
                                    unsigned int &length,
                                    unsigned char **bytes)
{
    SessionInfoData sessionInfoData(sessionInfo);
    
    if (sessionInfoData.isValid())
    {
        length = sessionInfoData.getLength();
        *bytes = (unsigned char*)malloc(length*sizeof(unsigned char));
        memcpy(*bytes, sessionInfoData.getData(), length*sizeof(unsigned char));
    }
    else
        length = 0;
}

bool
NdnRtcManager::deserializeSessionInfo(const unsigned int length,
                                      const unsigned char *bytes,
                                      SessionInfo &sessionInfo)
{
    SessionInfoData sessionInfoData(length, bytes);
    
    if (sessionInfoData.isValid())
    {
        sessionInfoData.getSessionInfo(sessionInfo);
        return true;
    }
    
    return false;
}

void
NdnRtcManager::fatalException(const std::exception& e)
{
    LogError(LIB_LOG)
    << "Fatal exception occurred: "
    << boost::diagnostic_information(e) << std::endl;
    
    LogError(LIB_LOG) << "Performing cleanup due to " << e.what() << std::endl;
    
    failed_ = true;
    failure();
    
    NdnRtcUtils::dispatchOnBackgroundThread([e](){
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_FATAL, e.what());
    });
}

//********************************************************************************
#pragma mark - private
int NdnRtcManager::notifyObserverWithError(const char *format, ...) const
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    notifyObserver("error", emsg);
    
    return RESULT_ERR;
}

int NdnRtcManager::notifyObserverWithState(const char *stateName, const char *format, ...) const
{
    va_list args;
    
    static char msg[256];
    
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);
    
    notifyObserver(stateName, msg);
    
    return RESULT_OK;
}

void NdnRtcManager::notifyObserver(const char *state, const char *args) const
{
    LibraryInternalObserver.onStateChanged(state, args);
}

//******************************************************************************
void init()
{
    Logger::initAsyncLogging();
    Logger::getLogger(LIB_LOG).setLogLevel(NdnLoggerDetailLevelDefault);
    LogInfo(LIB_LOG) << "NDN-RTC " << PACKAGE_VERSION << std::endl;
    
    struct sigaction act;
    
    memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &signalHandler;
    act.sa_flags = SA_SIGINFO;
    
    sigaction(SIGPIPE, &act, NULL);
    
    reset();
    
    LogInfo(LIB_LOG) << "Init voice engine..." << std::endl;
    NdnRtcUtils::initVoiceEngine();
    LogInfo(LIB_LOG) << "Voice engine initialized" << std::endl;
}

void cleanup()
{
    LogInfo(LIB_LOG) << "Stopping active session " << ActiveSession->getPrefix() << std::endl;
    ActiveSession.reset();
    
    LogInfo(LIB_LOG) << "Stopping recovery timer..." << std::endl;
    recoveryCheckTimer.cancel();
    LogInfo(LIB_LOG) << "Recovery timer stopped" << std::endl;
    
    LogInfo(LIB_LOG) << "Stopping active consumers..." << std::endl;
    {
        for (auto consumerIt:ActiveStreamConsumers)
            consumerIt.second->stop();
        ActiveStreamConsumers.clear();
    }
    LogInfo(LIB_LOG) << "Active consumers cleared" << std::endl;
    
    NdnRtcUtils::destroyLibFace();
    
    LogInfo(LIB_LOG) << "Stopping voice thread..." << std::endl;
    NdnRtcUtils::releaseVoiceEngine();
    LogInfo(LIB_LOG) << "Releasing voice engine..." << std::endl;
    NdnRtcUtils::stopBackgroundThread();
    LogInfo(LIB_LOG) << "Bye" << std::endl;
    Logger::releaseAsyncLogging();
}

void reset()
{
    NdnRtcUtils::startBackgroundThread();
    
    LogInfo(LIB_LOG) << "Starting recovery timer..." << std::endl;
    recoveryCheckTimer.expires_from_now(chrono::milliseconds(50));
    recoveryCheckTimer.async_wait(recoveryCheck);
    LogInfo(LIB_LOG) << "Recovery timer started" << std::endl;
}

void failure()
{
    // shutting down consuming part
    recoveryCheckTimer.cancel();
    for (auto consumerIt:ActiveStreamConsumers)
        consumerIt.second->stop();
    ActiveStreamConsumers.clear();
    
    // shutting down producing part
    ActiveSession->invalidate();
}

void recoveryCheck(const boost::system::error_code& e)
{
    if (e != boost::asio::error::operation_aborted)
    {
        if (ActiveStreamConsumers.size())
        {
            for (auto it : ActiveStreamConsumers)
            {
                LogDebug(LIB_LOG) << "Recovery check for "
                << it.second->getPrefix() << "(state "
                << it.second->getState() << ")" << std::endl;
                
                int idleTime  = it.second->getIdleTime();
                
                if ((it.second->getState() == Consumer::StateFetching &&
                     idleTime > Consumer::MaxIdleTimeMs) ||
                    (it.second->getState() != Consumer::StateFetching &&
                     idleTime > Consumer::MaxChasingTimeMs))
                {
                    LogWarn(LIB_LOG)
                    << "Idle time " << idleTime
                    << " (consumer state " << it.second->getState() << ") "
                    << ". Rebuffering triggered for "
                    << it.second->getPrefix() << std::endl;
                    
                    it.second->triggerRebuffering();
                }
            }
        }
        
        try {
            recoveryCheckTimer.expires_from_now(boost::chrono::milliseconds(50));
            recoveryCheckTimer.async_wait(recoveryCheck);
        } catch (boost::system::system_error& e) {
            LogError(LIB_LOG) << "Exception while recovery timer reset: "
            << e.what() << std::endl;
        }
    }
    else
        LogInfo(LIB_LOG) << "Recovery checks aborted" << std::endl;
}
