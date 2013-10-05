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
#include <memory>

using namespace ndnrtc;

static shared_ptr<NdnSenderChannel> senderChannel;

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
int NdnRtcLibrary::startConference(NdnParams &params)
{
    shared_ptr<NdnParams> defaultParams(NdnSenderChannel::defaultParams());
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(defaultParams.get()));
    
    sc->setObserver(this);
    
    if (sc->init() < 0)
        return notifyObserverWithError("can't initialize sender channel");
    
    if (sc->startTransmission() < 0)
        return notifyObserverWithError("can't start transmission");
    
    senderChannel_ = sc;
    return 0;
}

int NdnRtcLibrary::joinConference(const char *conferencePrefix)
{
    TRACE("join conference with prefix %s", conferencePrefix);
    return 0;
}

int NdnRtcLibrary::leaveConference(const char *conferencePrefix)
{
    TRACE("leaving conference with prefix: %s", conferencePrefix);
    if (senderChannel_.get())
        sc->stopTransmission();
    
    
    return 0;
}

void NdnRtcLibrary::onErrorOccurred(const char *errorMessage)
{
    TRACE("error occurred");
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
    
    return 0;
}
void NdnRtcLibrary::notifyObserver(const char *state, const char *args)
{
    if (observer_)
        observer_->onStateChanged(state, args);
}
