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
#include "media-receiver.h"

namespace ndnrtc
{
    class IAudioPacketConsumer
    {
    public:
        virtual void onRTPPacketReceived(unsigned int len, unsigned char *data) = 0;
        virtual void onRTCPPacketReceived(unsigned int len, unsigned char *data) = 0;
    };
    
    class NdnAudioReceiver : public NdnMediaReceiver
    {
    public:
        NdnAudioReceiver(const ParamsStruct &params);
        ~NdnAudioReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        
        void setFrameConsumer(IAudioPacketConsumer *consumer) {
            packetConsumer_ = consumer;
        }
        
    private:
        IAudioPacketConsumer *packetConsumer_;
        
        // overriden
        void playbackPacket(int64_t packetTsLocal);
        bool isLate(unsigned int frameNo);
    };
}

#endif /* defined(__ndnrtc__audio_receiver__) */
