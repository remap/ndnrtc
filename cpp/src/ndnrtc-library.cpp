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

static shared_ptr<NdnSenderChannel> SenderChannel(nullptr);
static map<string, shared_ptr<NdnReceiverChannel>> Producers;

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
  static int initialized = 0;
  if (!initialized) {
    initialized = 1;
    NdnLogger::initialize(DefaultParams.logFile, DefaultParams.loggingLevel);
    INFO("module loaded");
  }
}

__attribute__((destructor))
static void destructor(){
  INFO("module unloaded");
}

extern "C" NdnRtcLibrary* create_ndnrtc(void *libHandle)
{
  signal(SIGPIPE, SIG_IGN);
  return new NdnRtcLibrary(libHandle);
}

extern "C" void destroy_ndnrtc( NdnRtcLibrary* object )
{
  if (SenderChannel.get())
    object->stopPublishing();
  
  if (Producers.size())
  {
    map<string, shared_ptr<NdnReceiverChannel>>::iterator it;
    
    for (it = Producers.begin(); it != Producers.end(); it++)
    {
      shared_ptr<NdnReceiverChannel> producer = it->second;
      producer->stopTransmission();
    }
  }
  
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


//******************************************************************************
#pragma mark - construction/destruction
NdnRtcLibrary::NdnRtcLibrary(void *libHandle):
observer_(NULL),
libraryHandle_(libHandle),
libParams_(DefaultParams),
libAudioParams_(DefaultParamsAudio)
{
  TRACE("");
  NdnRtcUtils::sharedVoiceEngine();
}
NdnRtcLibrary::~NdnRtcLibrary()
{
  TRACE("");
  NdnRtcUtils::releaseVoiceEngine();
}
//******************************************************************************
#pragma mark - public
void NdnRtcLibrary::configure(const ParamsStruct &params,
                              const ParamsStruct &audioParams)
{
  ParamsStruct validatedVideoParams, validatedAudioParams;
  
  bool wasModified = false;
  int res = ParamsStruct::validateVideoParams(params, validatedVideoParams);
  
  if (RESULT_FAIL(res))
  {
    notifyObserverWithError("error", "bad video parameteres");
    return;
  }
  
  wasModified = RESULT_WARNING(res);
  res = ParamsStruct::validateAudioParams(audioParams, validatedAudioParams);
  
  if (RESULT_FAIL(res))
  {
    notifyObserverWithError("bad audio parameters");
    return;
  }
  
  wasModified |= RESULT_WARNING(res);
  
  NdnLogger::initialize(validatedVideoParams.logFile, validatedVideoParams.loggingLevel);
  
  libParams_ = validatedVideoParams;
  libAudioParams_ = validatedAudioParams;
  
  if (wasModified)
    notifyObserverWithState("warn", "some parameters were malformed. using default"
                            " instead");
  else
    notifyObserverWithState("init", "initialized with new parameters");
}
void NdnRtcLibrary::currentParams(ParamsStruct &params,
                                  ParamsStruct &audioParams)
{
  params = libParams_;
  audioParams = libAudioParams_;
}

void NdnRtcLibrary::getDefaultParams(ParamsStruct &videoParams,
                                     ParamsStruct &audioParams) const
{
  videoParams = DefaultParams;
  audioParams = DefaultParamsAudio;
}

int NdnRtcLibrary::getStatistics(const char *conferencePrefix,
                                 NdnLibStatistics &stat) const
{
  if (SenderChannel.get())
  {
    SenderChannel->getChannelStatistics(stat.sendStat_);
#if 0
    stat.sentNo_ = SenderChannel->getSentFramesNum();
    stat.sendingFramesFreq_ = SenderChannel->getNInputFramesPerSec();
    stat.capturingFreq_ = SenderChannel->getCurrentCapturingFreq();
#endif
  }
  
  if (!conferencePrefix || Producers.find(string(conferencePrefix)) == Producers.end())
    return -1; //notifyObserverWithError("producer was not found");
  
  shared_ptr<NdnReceiverChannel> producer = Producers[string(conferencePrefix)];
  
  stat.producerId_ = conferencePrefix;
  producer->getChannelStatistics(stat.receiveStat_);
#if 0
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
#endif
  return 0;
}

int NdnRtcLibrary::startPublishing(const char *username)
{
  ParamsStruct params = libParams_;
  ParamsStruct audioParams = libAudioParams_;
  
  if (username)
  {
    if (strcmp(username, "") == 0)
      return notifyObserverWithError("username cannot be empty string");
    
    params.producerId = username;
    audioParams.producerId = username;
  }
  
  if (SenderChannel.get())
    stopPublishing();
  
  shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(params, audioParams));
  
  sc->setObserver(this);
  
  if (RESULT_FAIL(sc->init()))
    return -1;
  
  if (RESULT_FAIL(sc->startTransmission()))
    return -1;
  
  SenderChannel = sc;
  
  string producerPrefix, framePrefix;
  MediaSender::getUserPrefix(params, producerPrefix);
  MediaSender::getStreamFramePrefix(params, framePrefix);
  
  return notifyObserverWithState("transmitting",
                                 "started publishing under the user prefix: %s",
                                 producerPrefix.c_str());
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
  if (strcmp(conferencePrefix, "") == 0)
    return notifyObserverWithError("username cannot be empty string");
  
  TRACE("join conference with prefix %s", conferencePrefix);
  
  if (Producers.find(string(conferencePrefix)) != Producers.end())
    return notifyObserverWithError("already joined conference");
  
  // setup params
  ParamsStruct params = libParams_;
  ParamsStruct audioParams = libAudioParams_;
  
  params.producerId = conferencePrefix;
  audioParams.producerId = conferencePrefix;
  
  shared_ptr<NdnReceiverChannel> producer(new
                                          NdnReceiverChannel(params, audioParams));
  
  producer->setObserver(this);
  
  if (RESULT_FAIL(producer->init()))
    return -1;
  
  if (RESULT_FAIL(producer->startTransmission()))
    return -1;
  
  Producers[string(conferencePrefix)] = producer;
  
  string producerPrefix;
  MediaSender::getUserPrefix(params, producerPrefix);
  
  return notifyObserverWithState("fetching",
                                 "fetching from the user %s",
                                 producerPrefix.c_str());
}

int NdnRtcLibrary::leaveConference(const char *conferencePrefix)
{
  TRACE("leaving conference with prefix: %s", conferencePrefix);
  
  if (Producers.find(string(conferencePrefix)) == Producers.end())
    return notifyObserverWithError("didn't find a conference to leave. did you join?");
  
  shared_ptr<NdnReceiverChannel> producer = Producers[string(conferencePrefix)];
  
  if (producer->stopTransmission() < 0)
    notifyObserverWithError("can't leave the conference");
  
  Producers.erase(string(conferencePrefix));
  
  return notifyObserverWithState("leave", "left producer %s", conferencePrefix);
}

void NdnRtcLibrary::onErrorOccurred(const char *errorMessage)
{
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
  if (observer_)
    observer_->onStateChanged(state, args);
}
