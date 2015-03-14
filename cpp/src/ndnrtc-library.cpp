//
//  ndnrtc-library.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <signal.h>
#include <boost/thread/mutex.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include "ndnrtc-library.h"
#include "ndnrtc-namespace.h"
#include "audio-consumer.h"
#include "video-consumer.h"
#include "external-capturer.hpp"
#include "session.h"
#include "error-codes.h"

using namespace boost;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

static std::string LIB_LOG = "ndnrtc.log";

static shared_ptr<FaceProcessor> LibraryFace;

typedef std::map<std::string, shared_ptr<Session>> SessionMap;
static SessionMap ActiveSessions;

typedef std::map<std::string, shared_ptr<Consumer>> ConsumerStreamMap;
static ConsumerStreamMap ActiveStreamConsumers;

typedef std::map<std::string, shared_ptr<ServiceChannel>> SessionObserverMap;
static SessionObserverMap RemoteObservers;

static boost::thread backgroundThread;
static boost::asio::io_service io_service;
static shared_ptr<boost::asio::io_service::work> backgroundWork;
static boost::asio::steady_timer recoveryCheckTimer(io_service);
static boost::mutex recoveryCheckMutex;
typedef boost::lock_guard<boost::mutex> ScopedLock;

void recoveryCheck(const boost::system::error_code& e);
std::string getFullLogPath(const GeneralParams& generalParams, std::string fileName);

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
    if (!initialized) {
        initialized = 1;
    }
}

__attribute__((destructor))
static void destructor(){
}

//******************************************************************************
static void signalHandler(int signal, siginfo_t *siginfo, void *context)
{
    LogInfo(LIB_LOG) << "Received signal " << signal << std::endl;
    
    if (signal == SIGPIPE)
    {
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_SIGPIPE,
                                                "Make sure your NDN daemon is running");
    }
}

//******************************************************************************
#pragma mark - construction/destruction
NdnRtcLibrary::NdnRtcLibrary(void *libHandle)
{
    Logger::getLogger(LIB_LOG).setLogLevel(NdnLoggerDetailLevelDefault);
    LogInfo(LIB_LOG) << "NDN-RTC " << PACKAGE_VERSION << std::endl;
    
    struct sigaction act;
    
    memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &signalHandler;
    act.sa_flags = SA_SIGINFO;
    
    sigaction(SIGPIPE, &act, NULL);
//    fclose(stderr);
    
    LogInfo(LIB_LOG) << "Voice engine initialization..." << std::endl;
    NdnRtcUtils::sharedVoiceEngine();
    LogInfo(LIB_LOG) << "Voice engine initialized" << std::endl;
    
    LogInfo(LIB_LOG) << "Starting recovery timer..." << std::endl;
    recoveryCheckTimer.expires_from_now(boost::chrono::milliseconds(50));
    recoveryCheckTimer.async_wait(recoveryCheck);
    LogInfo(LIB_LOG) << "Recovery timer started" << std::endl;
    
    LogInfo(LIB_LOG) << "Starting background thread..." << std::endl;
    backgroundWork.reset(new boost::asio::io_service::work(io_service));
    backgroundThread = boost::thread([](){
        LogInfo(LIB_LOG) << "Background thread "
        << boost::this_thread::get_id() << " started" << std::endl;
        
        io_service.run();
        
        LogInfo(LIB_LOG) << "Background thread stopped" << std::endl;
    });
    
}
NdnRtcLibrary::~NdnRtcLibrary()
{
    LogInfo(LIB_LOG) << "NDN-RTC Destructor" << std::endl;
    
    try {
        LogInfo(LIB_LOG) << "Stopping recovery timer..." << std::endl;
        recoveryCheckTimer.cancel();
        LogInfo(LIB_LOG) << "Recovery timer stopped" << std::endl;
        backgroundWork.reset();
        io_service.stop();
        backgroundThread.try_join_for(boost::chrono::milliseconds(500));
    }
    catch (boost::system::system_error& e) {
        LogError(LIB_LOG) << "Exception while stopping timer: "
        << e.what() << std::endl;
    }
    
    LogInfo(LIB_LOG) << "Stopping active sessions..." << std::endl;
    
    for (SessionMap::iterator it = ActiveSessions.begin();
         it != ActiveSessions.end(); it++)
    {
        LogInfo(LIB_LOG) << "Stopping " << it->second->getPrefix() << std::endl;
        it->second->stop();
    }
    ActiveSessions.clear();
    
    LogInfo(LIB_LOG) << "Active sessions cleared" << std::endl;
    LogInfo(LIB_LOG) << "Stopping remote session observers..." << std::endl;
    
    for (SessionObserverMap::iterator it = RemoteObservers.begin();
         it != RemoteObservers.end(); it++)
    {
        LogInfo(LIB_LOG) << "Stopping " << it->second->getPrefix() << std::endl;
        it->second->stopMonitor();
    }
    RemoteObservers.clear();
    
    LogInfo(LIB_LOG) << "Active remote session observers stopped" << std::endl;
    
    if (LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Stopping library Face..." << std::endl;
        LibraryFace->stopProcessing();
        LogInfo(LIB_LOG) << "Library face stopped" << std::endl;
    }
    
    LogInfo(LIB_LOG) << "Releasing voice engine..." << std::endl;
    
    NdnRtcUtils::releaseVoiceEngine();
    
    LogInfo(LIB_LOG) << "Bye" << std::endl;
}
//******************************************************************************
#pragma mark - public
void NdnRtcLibrary::setObserver(INdnRtcLibraryObserver *observer)
{
    LogInfo(LIB_LOG) << "Set library observer " << observer << std::endl;
    
    LibraryInternalObserver.setLibraryObserver(observer);
}

//******************************************************************************
std::string NdnRtcLibrary::startSession(const std::string& username,
                                        const new_api::GeneralParams& generalParams,
                                        ISessionObserver *sessionObserver)
{
    LIB_LOG = getFullLogPath(generalParams, generalParams.logFile_);
    
    Logger::getLogger(LIB_LOG).setLogLevel(generalParams.loggingLevel_);
    LogInfo(LIB_LOG) << "Starting session for user " << username << "..." << std::endl;
    
    std::string sessionPrefix = *NdnRtcNamespace::getProducerPrefix(generalParams.prefix_, username);
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
    {
        LogInfo(LIB_LOG) << "Creating new session instance..." << std::endl;
        
        shared_ptr<Session> session(new Session());
        session->setSessionObserver(sessionObserver);
        session->registerCallback(&LibraryInternalObserver);
        
        if (RESULT_NOT_FAIL(session->init(username, generalParams)))
        {
            ActiveSessions[session->getPrefix()] = session;
            session->start();
            sessionPrefix = session->getPrefix();
        }
        else
        {
            LogError(LIB_LOG) << "Session initialization failed" << std::endl;
            return "";
        }
    }
    else
    {
        LogInfo(LIB_LOG) << "Old session instance re-used..." << std::endl;
        
        it->second->init(username, generalParams);
        it->second->start();
    }
    
    LogInfo(LIB_LOG) << "Session started (prefix " << sessionPrefix
    << ")" << std::endl;
    
    return sessionPrefix;
}

int NdnRtcLibrary::stopSession(const std::string& userPrefix)
{
    LogInfo(LIB_LOG) << "Stopping session for prefix " << userPrefix << std::endl;
    
    int res = RESULT_ERR;
    
    SessionMap::iterator it = ActiveSessions.find(userPrefix);
    if (it != ActiveSessions.end())
    {
        res = it->second->stop();
        ActiveSessions.erase(it);
        
        LogInfo(LIB_LOG) << "Session was succesfully stopped" << std::endl;
    }
    else
        LogError(LIB_LOG) << "Session was not found" << std::endl;
    
    return res;
}

std::string
NdnRtcLibrary::setRemoteSessionObserver(const std::string& username,
                                        const std::string& prefix,
                                        const new_api::GeneralParams& generalParams,
                                        IRemoteSessionObserver* sessionObserver)
{
    LogInfo(LIB_LOG) << "Setting remote session observer " << sessionObserver
    << " for username " << username
    << " and prefix " << prefix << "..." << std::endl;
    
    if (!LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Creating library Face..." << std::endl;
        
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_);
        LibraryFace->startProcessing();
        
        LogInfo(LIB_LOG) << "Library face created succesfully" << std::endl;
    }
    
    std::string sessionPrefix = *NdnRtcNamespace::getProducerPrefix(prefix, username);
    SessionObserverMap::iterator it = RemoteObservers.find(sessionPrefix);
    
    if (it == RemoteObservers.end())
    {
        shared_ptr<ServiceChannel> remoteSessionChannel(new ServiceChannel(sessionObserver, LibraryFace));
        remoteSessionChannel->startMonitor(sessionPrefix);
        
        RemoteObservers[sessionPrefix] = remoteSessionChannel;
        
        LogInfo(LIB_LOG) << "Remote observer started for session prefix " << sessionPrefix << std::endl;
        return sessionPrefix;
    }
    else
    {
        LogError(LIB_LOG) << "Observer already exists" << std::endl;
    }
    
    return "";
}

int NdnRtcLibrary::removeRemoteSessionObserver(const std::string& sessionPrefix)
{
    LogInfo(LIB_LOG) << "Removing remote session observer for prefix "
    << sessionPrefix << "..." << std::endl;
    
    SessionObserverMap::iterator it = RemoteObservers.find(sessionPrefix);
    
    if (it == RemoteObservers.end())
    {
        LogError(LIB_LOG) << "Observer was not found" << std::endl;
        return RESULT_ERR;
    }
    
    it->second->stopMonitor();
    RemoteObservers.erase(it);
    
    LogInfo(LIB_LOG) << "Observer stopped" << std::endl;
    
    if (RemoteObservers.size() == 0)
    {
        LogInfo(LIB_LOG) << "No more observers. Shutting down library Face..." << std::endl;
        
        LibraryFace->stopProcessing();
        LibraryFace.reset();
        
        LogInfo(LIB_LOG) << "Library Face shut down" << std::endl;
    }
    
    return RESULT_OK;
}

std::string NdnRtcLibrary::addLocalStream(const std::string& sessionPrefix,
                                          const new_api::MediaStreamParams& params,
                                          IExternalCapturer** const capturer)
{
    LogInfo(LIB_LOG) << "Adding new stream for session "
    << sessionPrefix << "..." << std::endl;
    
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
    {
        LogError(LIB_LOG) << "Session was not found" << std::endl;
        return "";
    }
    
    std::string streamPrefix = "";

    if (RESULT_NOT_FAIL(it->second->addLocalStream(params, capturer, streamPrefix)))
    {
        LogInfo(LIB_LOG) << "Succesfully added stream with prefix "
        << streamPrefix << std::endl;
    }
    else
        LogError(LIB_LOG) << "Failed to add new stream" << std::endl;
    
    return streamPrefix;
}

int NdnRtcLibrary::removeLocalStream(const std::string& sessionPrefix,
                                     const std::string& streamPrefix)
{
    LogInfo(LIB_LOG) << "Removing stream " << streamPrefix
    << " from session " << sessionPrefix << "..." << std::endl;
    
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
    {
        LogError(LIB_LOG) << "Session was not found" << std::endl;
        return RESULT_ERR;
    }

    it->second->removeLocalStream(streamPrefix);
    LogInfo(LIB_LOG) << "Stream was removed succesfully" << std::endl;
    
    return RESULT_OK;
}

std::string
NdnRtcLibrary::addRemoteStream(const std::string& remoteSessionPrefix,
                               const new_api::MediaStreamParams& params,
                               const new_api::GeneralParams& generalParams,
                               const new_api::GeneralConsumerParams& consumerParams,
                               IExternalRenderer* const renderer)
{
    LogInfo(LIB_LOG) << "Adding remote stream for session "
    << remoteSessionPrefix << "..." << std::endl;
    
    if (!LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Creating library Face..." << std::endl;
        
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_);
        LibraryFace->startProcessing();
        
        LogInfo(LIB_LOG) << "Library Face created" << std::endl;
    }
    
    std::string streamPrefix = *NdnRtcNamespace::getStreamPrefix(remoteSessionPrefix, params.streamName_);
    shared_ptr<Consumer> remoteStreamConsumer;
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it != ActiveStreamConsumers.end() &&
        it->second->getIsConsuming())
    {
        LogWarn(LIB_LOG) << "Stream was already added" << std::endl;
        
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_ALREADY_EXISTS,
                                                "stream is already running");
        return "";
    }
    
    if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
        remoteStreamConsumer.reset(new AudioConsumer(generalParams, consumerParams));
    else
        remoteStreamConsumer.reset(new VideoConsumer(generalParams, consumerParams, renderer));
    
    if (it != ActiveStreamConsumers.end())
        it->second = remoteStreamConsumer;
    
    ConsumerSettings settings;
    settings.userPrefix_ = remoteSessionPrefix;
    settings.streamParams_ = params;
    settings.faceProcessor_ = LibraryFace;
    
    remoteStreamConsumer->registerCallback(&LibraryInternalObserver);
    
    if (RESULT_FAIL(remoteStreamConsumer->init(settings)))
    {
        LogError(LIB_LOG) << "Failed to initialize fetching from stream" << std::endl;
        return "";
    }
    
    std::string username = NdnRtcNamespace::getUserName(remoteSessionPrefix);
    std::string logFile = getFullLogPath(generalParams,
                                         NdnRtcUtils::toString("consumer-%s-%s.log",
                                                               username.c_str(),
                                                               params.streamName_.c_str()));
    
    remoteStreamConsumer->setLogger(new Logger(generalParams.loggingLevel_,
                                               logFile));
    
    if (RESULT_FAIL(remoteStreamConsumer->start()))
        return "";
    
    if (it == ActiveStreamConsumers.end())
    {
        ScopedLock lock(recoveryCheckMutex);
        ActiveStreamConsumers[remoteStreamConsumer->getPrefix()] = remoteStreamConsumer;
    }
    
    return remoteStreamConsumer->getPrefix();
}

std::string
NdnRtcLibrary::removeRemoteStream(const std::string& streamPrefix)
{
    LogInfo(LIB_LOG) << "Removing stream " << streamPrefix << "..." << std::endl;
    
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LogError(LIB_LOG) << "Stream was not added previously" << std::endl;
        return "";
    }
    
    std::string logFileName = it->second->getLogger()->getFileName();
    it->second->stop();
    {
        ScopedLock lock(recoveryCheckMutex);
        ActiveStreamConsumers.erase(it);
    }
    
    LogInfo(LIB_LOG) << "Stream removed succesfully" << std::endl;
    
    return logFileName;
}

int
NdnRtcLibrary::setStreamObserver(const std::string& streamPrefix,
                                 IConsumerObserver* const observer)
{
    LogInfo(LIB_LOG) << "Setting stream observer " << observer
    << " for stream " << streamPrefix << "..." << std::endl;
    
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LogError(LIB_LOG) << "Stream was not added previously" << std::endl;
        return RESULT_ERR;
    }

    it->second->registerObserver(observer);
    LogInfo(LIB_LOG) << "Added observer succesfully" << std::endl;
    
    return RESULT_OK;
}

int
NdnRtcLibrary::removeStreamObserver(const std::string& streamPrefix)
{
    LogInfo(LIB_LOG) << "Removing stream observer for prefix " << streamPrefix
    << "..." << std::endl;
    
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LogError(LIB_LOG) << "Couldn't find requested stream" << std::endl;
        return RESULT_ERR;
    }
    
    it->second->unregisterObserver();
    LogInfo(LIB_LOG) << "Stream observer was removed succesfully" << std::endl;
    
    return RESULT_OK;
}

std::string
NdnRtcLibrary::getStreamThread(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_NOT_FOUND,
                                                "stream was not found");
        return "";
    }
    
    return it->second->getCurrentThreadName();
}

int
NdnRtcLibrary::switchThread(const std::string& streamPrefix,
                            const std::string& threadName)
{
    LogInfo(LIB_LOG) << "Switching to thread " << threadName
    << " for stream " << streamPrefix << std::endl;
    
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LogError(LIB_LOG) << "Couldn't find requested stream" << std::endl;
        return RESULT_ERR;
    }
    
    it->second->switchThread(threadName);
    LogInfo(LIB_LOG) << "Thread switched succesfully" << std::endl;
    
    return RESULT_OK;
}

statistics::StatisticsStorage
NdnRtcLibrary::getRemoteStreamStatistics(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamConsumers.find(streamPrefix);
    
    if (it == ActiveStreamConsumers.end())
    {
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_NOT_FOUND,
                                                "stream was not found");
        return *statistics::StatisticsStorage::createConsumerStatistics();
    }
    
    return it->second->getStatistics();;
}

//******************************************************************************
void
NdnRtcLibrary::getVersionString(char **versionString)
{
    if (versionString)
        memcpy((void*)versionString, PACKAGE_VERSION, strlen(PACKAGE_VERSION));
               
    return;
}

//********************************************************************************
#pragma mark - private
int NdnRtcLibrary::notifyObserverWithError(const char *format, ...) const
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    notifyObserver("error", emsg);
    
    return RESULT_ERR;
}

int NdnRtcLibrary::notifyObserverWithState(const char *stateName, const char *format, ...) const
{
    va_list args;
    
    static char msg[256];
    
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);
    
    notifyObserver(stateName, msg);
    
    return RESULT_OK;
}

void NdnRtcLibrary::notifyObserver(const char *state, const char *args) const
{
    LibraryInternalObserver.onStateChanged(state, args);
}

//******************************************************************************
void recoveryCheck(const boost::system::error_code& e)
{
    if (e != boost::asio::error::operation_aborted)
    {
        ScopedLock lock(recoveryCheckMutex);
        
        if (ActiveStreamConsumers.size())
        {
            for (auto it : ActiveStreamConsumers)
            {
                LogDebug(LIB_LOG) << "Recovery check for "
                << it.second->getPrefix() << std::endl;
                
                int idleTime  = it.second->getIdleTime();
                if (idleTime > Consumer::MaxIdleTimeMs)
                {
                    LogWarn(LIB_LOG)
                    << "Idle time " << idleTime
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

std::string getFullLogPath(const GeneralParams& generalParams, std::string fileName)
{
    static char logPath[PATH_MAX];
    return ((generalParams.logPath_ == "")?std::string(getwd(logPath)):generalParams.logPath_) + "/" + fileName;
}