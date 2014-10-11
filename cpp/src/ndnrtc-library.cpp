//
//  ndnrtc-library.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <stdlib.h>
#include <string.h>
#include <memory>
#include <signal.h>

#include "ndnrtc-library.h"
#include "ndnrtc-namespace.h"
#include "consumer-channel.h"
#include "objc/cocoa-renderer.h"
#include "external-capturer.hpp"
#include "session.h"
#include "error-codes.h"

using namespace boost;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

static shared_ptr<FaceProcessor> LibraryFace;

typedef std::map<std::string, shared_ptr<Session>> SessionMap;
static SessionMap ActiveSessions;

typedef std::map<std::string, shared_ptr<Consumer>> ConsumerStreamMap;
static ConsumerStreamMap ActiveStreamsConsumer;

typedef std::map<std::string, shared_ptr<ServiceChannel>> SessionObserverMap;
static SessionObserverMap RemoteObservers;

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
    struct sigaction act;
    
    memset (&act, '\0', sizeof(act));
    act.sa_sigaction = &signalHandler;
    act.sa_flags = SA_SIGINFO;
    
    sigaction(SIGPIPE, &act, NULL);
//    fclose(stderr);    
    NdnRtcUtils::sharedVoiceEngine();
}
NdnRtcLibrary::~NdnRtcLibrary()
{
    for (SessionMap::iterator it = ActiveSessions.begin();
         it != ActiveSessions.end(); it++)
    {
        it->second->stop();
    }
    ActiveSessions.clear();
    
    for (SessionObserverMap::iterator it = RemoteObservers.begin();
         it != RemoteObservers.end(); it++)
    {
        it->second->stopMonitor();
    }
    RemoteObservers.clear();
    
    LibraryFace->stopProcessing();
    NdnRtcUtils::releaseVoiceEngine();
}
//******************************************************************************
#pragma mark - public
void NdnRtcLibrary::setObserver(INdnRtcLibraryObserver *observer)
{
    LibraryInternalObserver.setLibraryObserver(observer);
}

//******************************************************************************
std::string NdnRtcLibrary::startSession(const std::string& username,
                                        const new_api::GeneralParams& generalParams,
                                        ISessionObserver *sessionObserver)
{
    std::string sessionPrefix = *NdnRtcNamespace::getProducerPrefix(generalParams.prefix_, username);
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
    {
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
            return "";
    }
    else
    {
        it->second->init(username, generalParams);
        it->second->start();
    }
    
    return sessionPrefix;
}

int NdnRtcLibrary::stopSession(const std::string& userPrefix)
{
    int res = RESULT_ERR;
    
    SessionMap::iterator it = ActiveSessions.find(userPrefix);
    if (it != ActiveSessions.end())
    {
        res = it->second->stop();
        ActiveSessions.erase(it);        
    }
    
    return res;
}

std::string
NdnRtcLibrary::setRemoteSessionObserver(const std::string& username,
                                        const std::string& prefix,
                                        const new_api::GeneralParams& generalParams,
                                        IRemoteSessionObserver* sessionObserver)
{
    if (!LibraryFace.get())
    {
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_);
        LibraryFace->startProcessing();
    }
    
    shared_ptr<ServiceChannel> remoteSessionChannel(new ServiceChannel(sessionObserver, LibraryFace));
    
    std::string sessionPrefix = *NdnRtcNamespace::getProducerPrefix(prefix, username);
    remoteSessionChannel->startMonitor(sessionPrefix);
    
    RemoteObservers[sessionPrefix] = remoteSessionChannel;
    
    return sessionPrefix;
}

int NdnRtcLibrary::removeRemoteSessionObserver(const std::string& sessionPrefix)
{
    SessionObserverMap::iterator it = RemoteObservers.find(sessionPrefix);
    
    if (it == RemoteObservers.end())
        return RESULT_ERR;
    
    it->second->stopMonitor();
    RemoteObservers.erase(it);
    
    if (RemoteObservers.size() == 0)
    {
        LibraryFace->stopProcessing();
        LibraryFace.reset();
    }
    
    return RESULT_OK;
}

std::string NdnRtcLibrary::addLocalStream(const std::string& sessionPrefix,
                                          const new_api::MediaStreamParams& params,
                                          IExternalCapturer** const capturer)
{
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
        return "";
    
    std::string streamPrefix = "";
    it->second->addLocalStream(params, capturer, streamPrefix);
    
    return streamPrefix;
}

int NdnRtcLibrary::removeLocalStream(const std::string& sessionPrefix,
                                     const std::string& streamPrefix)
{
    SessionMap::iterator it = ActiveSessions.find(sessionPrefix);
    
    if (it == ActiveSessions.end())
        return RESULT_ERR;

    it->second->removeLocalStream(streamPrefix);
    
    return RESULT_OK;
}

std::string
NdnRtcLibrary::addRemoteStream(const std::string& remoteSessionPrefix,
                               const new_api::MediaStreamParams& params,
                               const new_api::GeneralParams& generalParams,
                               const new_api::GeneralConsumerParams& consumerParams,
                               IExternalRenderer* const renderer)
{
    if (!LibraryFace.get())
    {
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_);
        LibraryFace->startProcessing();
    }
    
    std::string streamPrefix = *NdnRtcNamespace::getStreamPrefix(remoteSessionPrefix, params.streamName_);
    shared_ptr<Consumer> remoteStreamConsumer;
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it != ActiveStreamsConsumer.end() &&
        it->second->getIsConsuming())
    {
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_ALREADY_EXISTS,
                                                "stream is already running");
        return "";
    }
    
    if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
        remoteStreamConsumer.reset(new AudioConsumer(generalParams, consumerParams));
    else
        remoteStreamConsumer.reset(new VideoConsumer(generalParams, consumerParams, renderer));
    
    if (it != ActiveStreamsConsumer.end())
        it->second = remoteStreamConsumer;
    
    ConsumerSettings settings;
    settings.userPrefix_ = remoteSessionPrefix;
    settings.streamParams_ = params;
    settings.faceProcessor_ = LibraryFace;
    
    remoteStreamConsumer->registerCallback(&LibraryInternalObserver);
    
    if (RESULT_FAIL(remoteStreamConsumer->init(settings)))
        return "";
    
    remoteStreamConsumer->setLogger(new Logger(generalParams.loggingLevel_,
                                               NdnRtcUtils::toString("consumer-%s.log", params.streamName_.c_str())));
    
    if (RESULT_FAIL(remoteStreamConsumer->start()))
        return "";
    
    if (it == ActiveStreamsConsumer.end())
        ActiveStreamsConsumer[remoteStreamConsumer->getPrefix()] = remoteStreamConsumer;
    
    return remoteStreamConsumer->getPrefix();
}

int
NdnRtcLibrary::removeRemoteStream(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
        return RESULT_ERR;
    
    it->second->stop();
//    ActiveStreamsConsumer.erase(it);
    
    return RESULT_OK;
}

int
NdnRtcLibrary::setStreamObserver(const std::string& streamPrefix,
                                 IConsumerObserver* const observer)
{
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
        return RESULT_ERR;

    it->second->registerObserver(observer);
    
    return RESULT_OK;
}

int
NdnRtcLibrary::removeStreamObserver(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
        return RESULT_ERR;
    
    it->second->unregisterObserver();
    
    return RESULT_OK;
}

std::string
NdnRtcLibrary::getStreamThread(const std::string& streamPrefix)
{
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
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
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
        return RESULT_ERR;
    
    it->second->switchThread(threadName);
    
    return RESULT_OK;
}

int
NdnRtcLibrary::getRemoteStreamStatistics(const std::string& streamPrefix,
                                         ReceiverChannelPerformance& stat)
{
    ConsumerStreamMap::iterator it = ActiveStreamsConsumer.find(streamPrefix);
    
    if (it == ActiveStreamsConsumer.end())
    {
        LibraryInternalObserver.onErrorOccurred(NRTC_ERR_NOT_FOUND,
                                                "stream was not found");
        return NRTC_ERR_NOT_FOUND;
    }
    
    it->second->getStatistics(stat);
    return RESULT_OK;
}

//******************************************************************************
void
NdnRtcLibrary::getVersionString(char **versionString)
{
    if (versionString)
        memcpy((void*)versionString, PACKAGE_VERSION, strlen(PACKAGE_VERSION));
               
    return;
}

void
NdnRtcLibrary::arrangeWindows()
{
    arrangeAllWindows();
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
