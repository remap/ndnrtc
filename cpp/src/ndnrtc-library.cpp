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

#include <memory>

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
#pragma mark - construction/destruction

//********************************************************************************
#pragma mark - public
int NdnRtcLibrary::startConference(const char *username)
//int NdnRtcLibrary::startConference(NdnParams &params)
{
    shared_ptr<NdnParams> defaultParams(SenderChannelParams::defaultParams());
    
    if (username)
        defaultParams->setStringParam(VideoSenderParams::ParamNameProducerId, username);
    
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(defaultParams.get()));
    
    sc->setObserver(this);
    
    if (sc->init() < 0)
        return -1;
    
    if (sc->startTransmission() < 0)
        return -1;
    
    SenderChannel = sc;
    
    return notifyObserverWithState("transmitting",
                                   "started video translation under the user prefix: %s, video stream prefix: %s",
                                   ((VideoSenderParams*)defaultParams.get())->getUserPrefix().c_str(),
                                   ((VideoSenderParams*)defaultParams.get())->getStreamFramePrefix().c_str());
}

int NdnRtcLibrary::joinConference(const char *conferencePrefix)
{
    TRACE("join conference with prefix %s", conferencePrefix);
    
    if (Producers.find(string(conferencePrefix)) != Producers.end())
        return notifyObserverWithError("already joined conference");
    
    shared_ptr<ReceiverChannelParams> rp(ReceiverChannelParams::defaultParams());
    
    // setup params
    rp->setStringParam(VideoSenderParams::ParamNameProducerId, conferencePrefix);
    
    shared_ptr<NdnReceiverChannel> producer(new NdnReceiverChannel(rp.get()));
    
    producer->setObserver(this);
    
    if (producer->init() < 0)
        return -1;
    
    if (producer->startFetching() < 0)
        return -1;
    
    Producers[string(conferencePrefix)] = producer;
    
    return notifyObserverWithState("fetching",
                                   "fetching video from the prefix %s",
                                   ((VideoSenderParams*)rp.get())->getStreamFramePrefix().c_str());
}

int NdnRtcLibrary::leaveConference(const char *conferencePrefix)
{
    TRACE("leaving conference with prefix: %s", conferencePrefix);
//    if (SenderChannel.get())
//    {
//        SenderChannel->stopTransmission();
//        SenderChannel.reset();
//    }
    
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
int NdnRtcLibrary::notifyObserverWithError(const char *format, ...)
{
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    notifyObserver("error", emsg);
    
    return -1;
}
int NdnRtcLibrary::notifyObserverWithState(const char *stateName, const char *format, ...)
{
    va_list args;
    
    static char msg[256];
    
    va_start(args, format);
    vsprintf(msg, format, args);
    va_end(args);
    
    notifyObserver(stateName, msg);
    
    return 0;
}
void NdnRtcLibrary::notifyObserver(const char *state, const char *args)
{
    if (observer_)
        observer_->onStateChanged(state, args);
}
