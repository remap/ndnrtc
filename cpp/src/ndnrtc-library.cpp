//
//  ndnrtc-library.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "ndnrtc-library.h"
#include "sender-channel.h"
#include "receiver-channel.h"

#include <stdlib.h>
#include <string.h>
#include <memory>

#define CHECK_AND_SET_INT(paramSet, paramName, paramValue){ \
if ((int)paramValue >= 0) \
paramSet.setIntParam(paramName, paramValue); \
}

#define CHECK_AND_SET_STR(paramSet, paramName, paramValue){\
if (paramValue)\
paramSet.setStringParam(paramName, string(paramValue));\
}

using namespace ndnrtc;
using namespace std;

static shared_ptr<NdnSenderChannel> SenderChannel;
static map<string, shared_ptr<NdnReceiverChannel>> Producers;

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        NdnLogger::initialize("ndnrtc.log", NdnLoggerDetailLevelAll);
        INFO("module loaded");
    }
}

__attribute__((destructor))
static void destructor(){
    INFO("module unloaded");
}

extern "C" NdnRtcLibrary* create_ndnrtc(void *libHandle)
{
    return new NdnRtcLibrary(libHandle);
}

extern "C" void destroy_ndnrtc( NdnRtcLibrary* object )
{
    delete object;
}

//********************************************************************************
#pragma mark - all static
static const char *DefaultLogFile = NULL;

ParamsStruct NdnRtcLibrary::createParamsStruct()
{
    ParamsStruct params;
    memset(&params, 0, sizeof(ParamsStruct));
    return params;
}

void NdnRtcLibrary::releaseParamsStruct(ParamsStruct &params)
{
    if (params.logFile)
        free((void*)params.logFile);
    
    if (params.host)
        free((void*)params.host);
}


//********************************************************************************
#pragma mark - construction/destruction
NdnRtcLibrary::NdnRtcLibrary(void *libHandle):
observer_(NULL),
libraryHandle_(libHandle),
libParams_(DefaultParams)
{
}
//********************************************************************************
#pragma mark - public
void NdnRtcLibrary::configure(ParamsStruct &params)
{
    NdnLogger::initialize(params.logFile, params.loggingLevel);
    
    libParams_ = params;
    notifyObserverWithState("init", "initialized with new parameters");
}
ParamsStruct NdnRtcLibrary::currentParams()
{
    return libParams_;
}

ParamsStruct NdnRtcLibrary::getDefaultParams() const
{
    return DefaultParams;
}

int NdnRtcLibrary::getStatistics(const char *conferencePrefix, NdnLibStatistics &stat) const
{
    if (SenderChannel.get())
    {
        stat.sentNo_ = SenderChannel->getSentFramesNum();
        stat.sendingFramesFreq_ = SenderChannel->getNInputFramesPerSec();
        stat.capturingFreq_ = SenderChannel->getCurrentCapturingFreq();
    }
    
    if (!conferencePrefix || Producers.find(string(conferencePrefix)) == Producers.end())
        return -1; //notifyObserverWithError("producer was not found");
    
    shared_ptr<NdnReceiverChannel> producer = Producers[string(conferencePrefix)];
    
    stat.producerId_ = conferencePrefix;
    
    ReceiverChannelStatistics receiver_stat;
    producer->getStat(receiver_stat);
    
    stat.nPlayback_ = receiver_stat.nPlayback_;
    stat.nPipeline_ = receiver_stat.nPipeline_;
    stat.nFetched_ = receiver_stat.nFetched_;
    stat.nLate_ = receiver_stat.nLate_;
    stat.nTimeouts_ = receiver_stat.nTimeouts_;
    stat.nTotalTimeouts_ = receiver_stat.nTotalTimeouts_;
    stat.nSkipped_ = receiver_stat.nSkipped_;
    stat.nFree_ = receiver_stat.nFree_;
    stat.nLocked_ = receiver_stat.nLocked_;
    stat.nAssembling_ = receiver_stat.nAssembling_;
    stat.nNew_ = receiver_stat.nNew_;
    
    stat.inFramesFreq_ = receiver_stat.inFramesFreq_;
    stat.inDataFreq_ = receiver_stat.inDataFreq_;
    stat.playoutFreq_ = receiver_stat.playoutFreq_;
    
    return 0;
}

int NdnRtcLibrary::startPublishing(const char *username)
{
    ParamsStruct params = libParams_;
    
    if (username)
        params.producerId = username;
    
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(params));
    
    sc->setObserver(this);
    
    if (sc->init() < 0)
        return -1;
    
    if (sc->startTransmission() < 0)
        return -1;
    
    SenderChannel = sc;
    
    string producerPrefix, framePrefix;
    MediaSender::getUserPrefix(params, producerPrefix);
    MediaSender::getStreamFramePrefix(params, framePrefix);
    
    return notifyObserverWithState("transmitting",
                                   "started video translation under the user \
                                   prefix: %s, video stream prefix: %s",
                                   producerPrefix.c_str(),
                                   framePrefix.c_str());
}

int NdnRtcLibrary::stopPublishing()
{
    if (SenderChannel.get())
    {
        SenderChannel->stopTransmission();
        SenderChannel.reset();
    }
    
    return notifyObserverWithState("publishing stopped", "stopped publishing media");
}

int NdnRtcLibrary::joinConference(const char *conferencePrefix)
{
    TRACE("join conference with prefix %s", conferencePrefix);
    
    if (Producers.find(string(conferencePrefix)) != Producers.end())
        return notifyObserverWithError("already joined conference");

    // setup params
    ParamsStruct params = libParams_;
    
    params.producerId = conferencePrefix;
    
    shared_ptr<NdnReceiverChannel> producer(new NdnReceiverChannel(params));
    
    producer->setObserver(this);
    
    if (producer->init() < 0)
        return -1;
    
    if (producer->startFetching() < 0)
        return -1;
    
    Producers[string(conferencePrefix)] = producer;
    
    string producerPrefix;
    MediaSender::getStreamFramePrefix(params, producerPrefix);
    
    return notifyObserverWithState("fetching",
                                   "fetching video from the prefix %s",
                                   producerPrefix.c_str());
}

int NdnRtcLibrary::leaveConference(const char *conferencePrefix)
{
    TRACE("leaving conference with prefix: %s", conferencePrefix);
    
    if (Producers.find(string(conferencePrefix)) == Producers.end())
        return notifyObserverWithError("didn't find a conference to leave. did you join?");
    
    shared_ptr<NdnReceiverChannel> producer = Producers[string(conferencePrefix)];
    
    if (producer->stopFetching() < 0)
        notifyObserverWithError("can't leave the conference");
    
    Producers.erase(string(conferencePrefix));
    
    return notifyObserverWithState("leave", "left producer %s", conferencePrefix);
}

void NdnRtcLibrary::onErrorOccurred(const char *errorMessage)
{
    TRACE("error occurred");
    notifyObserverWithError(errorMessage);
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
    
    return -1;
}
int NdnRtcLibrary::notifyObserverWithState(const char *stateName, const char *format, ...) const
{
    va_list args;
    
    static char msg[256];
    
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);
    
    notifyObserver(stateName, msg);
    
    return 0;
}
void NdnRtcLibrary::notifyObserver(const char *state, const char *args) const
{
    if (observer_)
        observer_->onStateChanged(state, args);
}
