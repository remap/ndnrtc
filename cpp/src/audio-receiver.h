//
//  audio-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//


#ifndef __ndnrtc__audio_receiver__
#define __ndnrtc__audio_receiver__

#include "ndnrtc-common.h"

namespace ndnrtc
{
    class IAudioPacketConsumer
    {
    public:
        virtual void onRTPPacketReceived() = 0;
        virtual void onRTCPPacketReceived() = 0;
    };
    
    class NdnAudioReceiver
    {
    public:
        
        int init(shared_ptr<Face> face);
        
    private:
        IAudioPacketConsumer *packetConsumer_;
        
        
    };
}

#endif /* defined(__ndnrtc__audio_receiver__) */
