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

NdnLibParams NdnRtcLibrary::createParamsStruct()
{
    NdnLibParams paramsStruct;
    memset(&paramsStruct, 0, sizeof(NdnLibParams));
    return paramsStruct;
}

void NdnRtcLibrary::releaseParamsStruct(NdnLibParams &params)
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
libParams_(*ReceiverChannelParams::defaultParams())
{
    // setting up deafult params = receiver channel + sender channel params
    NdnParams *senderParams = SenderChannelParams::defaultParams();
    
    libParams_.addParams(*senderParams);
    
    delete senderParams;
}
//********************************************************************************
#pragma mark - public
void NdnRtcLibrary::configure(NdnLibParams &params)
{
    NdnLogger::initialize(params.logFile, params.loggingLevel);
    
    // capture settings
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameDeviceId, params.captureDeviceId)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameWidth, params.captureWidth)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameHeight, params.captureHeight)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameFPS, params.captureFramerate)
    
    // render
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameWindowWidth, params.renderWidth)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameWindowHeight, params.renderHeight)
    
    // codec
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameFrameRate, params.codecFrameRate)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameStartBitRate, params.startBitrate)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameMaxBitRate, params.maxBitrate)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameEncodeWidth, params.encodeWidth)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameEncodeHeight, params.encodeHeight)
    
    // network
    CHECK_AND_SET_STR(libParams_, NdnParams::ParamNameConnectHost, params.host)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameConnectPort, params.portNum)
    
    // ndn producer
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameSegmentSize, params.segmentSize)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameFrameFreshnessInterval, params.freshness)
    
    // ndn consumer
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameProducerRate, params.playbackRate)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameInterestTimeout, params.interestTimeout)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameFrameBufferSize, params.bufferSize)
    CHECK_AND_SET_INT(libParams_, NdnParams::ParamNameFrameSlotSize, params.slotSize)
    
    notifyObserverWithState("init", "initialized with parameters: %s", libParams_.description().c_str());
}
NdnLibParams NdnRtcLibrary::currentParams()
{
    NdnLibParams paramsStruct;
    
    memset(&paramsStruct, 0, sizeof(NdnLibParams));
    
    std::string logFName = NdnLogger::currentLogFile();
    
    if (logFName != "")
    {
        paramsStruct.logFile = (char*)malloc(256);
        memset(&paramsStruct.logFile, 0, 256);
        
        logFName.copy((char*)paramsStruct.logFile, logFName.size());
    }

    paramsStruct.loggingLevel = NdnLogger::currentLogLevel();
    
    // capture settings
    static_cast<CameraCapturerParams*>(&libParams_)->getDeviceId(&paramsStruct.captureDeviceId);
    static_cast<CameraCapturerParams*>(&libParams_)->getWidth(&paramsStruct.captureWidth);
    static_cast<CameraCapturerParams*>(&libParams_)->getHeight(&paramsStruct.captureHeight);
    static_cast<CameraCapturerParams*>(&libParams_)->getFPS(&paramsStruct.captureFramerate);
    
    // render
    static_cast<NdnRendererParams*>(&libParams_)->getWindowHeight(&paramsStruct.renderWidth);
    static_cast<NdnRendererParams*>(&libParams_)->getWindowHeight(&paramsStruct.renderHeight);
    
    // codec
    static_cast<NdnVideoCoderParams*>(&libParams_)->getFrameRate(&paramsStruct.codecFrameRate);
    static_cast<NdnVideoCoderParams*>(&libParams_)->getStartBitRate(&paramsStruct.startBitrate);
    static_cast<NdnVideoCoderParams*>(&libParams_)->getMaxBitRate(&paramsStruct.maxBitrate);
    static_cast<NdnVideoCoderParams*>(&libParams_)->getWidth(&paramsStruct.encodeWidth);
    static_cast<NdnVideoCoderParams*>(&libParams_)->getHeight(&paramsStruct.encodeHeight);
    
    // network
    std::string host = static_cast<SenderChannelParams*>(&libParams_)->getConnectHost();
    
    if (host != "")
    {
        paramsStruct.host = (char*)malloc(256);
        memset((void*)paramsStruct.host, 0, 256);
        
        host.copy((char*)paramsStruct.host, host.size());
    }
    
    paramsStruct.portNum = static_cast<SenderChannelParams*>(&libParams_)->getConnectPort();
    
    // ndn producer
    static_cast<VideoSenderParams*>(&libParams_)->getFreshnessInterval(&paramsStruct.freshness);
    static_cast<VideoSenderParams*>(&libParams_)->getSegmentSize(&paramsStruct.segmentSize);

    
    // ndn consumer
    paramsStruct.playbackRate = static_cast<VideoReceiverParams*>(&libParams_)->getProducerRate();
    paramsStruct.interestTimeout = static_cast<VideoReceiverParams*>(&libParams_)->getDefaultTimeout();
    paramsStruct.bufferSize = static_cast<VideoReceiverParams*>(&libParams_)->getFrameBufferSize();
    paramsStruct.slotSize = static_cast<VideoReceiverParams*>(&libParams_)->getFrameSlotSize();

    return paramsStruct;
}

NdnLibParams NdnRtcLibrary::getDefaultParams() const
{
    NdnLibParams defaultParams;
    
    memset(&defaultParams, -1, sizeof(defaultParams));
    
    defaultParams.loggingLevel = NdnLoggerDetailLevelDefault;
    defaultParams.logFile = DefaultLogFile;
    defaultParams.host = NULL;
    
    return defaultParams;
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
//int NdnRtcLibrary::startConference(NdnParams &params)
{
    NdnParams params(libParams_);
    
    if (username)
        params.setStringParam(VideoSenderParams::ParamNameProducerId, username);
    
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(&params));
    
    sc->setObserver(this);
    
    if (sc->init() < 0)
        return -1;
    
    if (sc->startTransmission() < 0)
        return -1;
    
    SenderChannel = sc;
    
    return notifyObserverWithState("transmitting",
                                   "started video translation under the user prefix: %s, video stream prefix: %s",
                                   static_cast<VideoSenderParams*>(&params)->getUserPrefix().c_str(),
//                                   ((VideoSenderParams)libParams_).getUserPrefix().c_str(),
                                   static_cast<VideoSenderParams*>(&params)->getStreamFramePrefix().c_str());
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
    NdnParams params(libParams_);
    
    params.setStringParam(VideoSenderParams::ParamNameProducerId, conferencePrefix);
    
    shared_ptr<NdnReceiverChannel> producer(new NdnReceiverChannel(&params));
    
    producer->setObserver(this);
    
    if (producer->init() < 0)
        return -1;
    
    if (producer->startFetching() < 0)
        return -1;
    
    Producers[string(conferencePrefix)] = producer;
    
    return notifyObserverWithState("fetching",
                                   "fetching video from the prefix %s",
                                   static_cast<VideoSenderParams*>(&params)->getStreamFramePrefix().c_str());
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
