//
//  ndnaudio-transport.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 10/21/13
//

#ifndef __ndnrtc__ndnaudio_transport__
#define __ndnrtc__ndnaudio_transport__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "audio-receiver.h"
#include "audio-sender.h"

//#define AUDIO_OFF // disable audio channel

namespace ndnrtc {
    class NdnAudioChannel : public NdnRtcObject,
    public webrtc::Transport
    {
    public:
        NdnAudioChannel(const ParamsStruct &params, webrtc::VoiceEngine *voiceEngine);
        virtual ~NdnAudioChannel();
        
        virtual int init(shared_ptr<Face> &face);
        virtual int start(); // starts transfering data (in/out)
        virtual int stop(); // stops transfering data (in/out)
        
    protected:
        int channel_;
        bool started_ = false, initialized_ = false;
        webrtc::VoEBase *voe_base_;
        webrtc::VoENetwork *voe_network_;
        
        // webrtc::Transport interface
        virtual int SendPacket(int channel, const void *data, int len);
        virtual int SendRTCPPacket(int channel, const void *data, int len);
    };
    
    class NdnAudioReceiveChannel : public NdnAudioChannel,
    public IAudioPacketConsumer
    {
    public:
        NdnAudioReceiveChannel(const ParamsStruct &params, webrtc::VoiceEngine *voiceEngine):
        NdnAudioChannel(params, voiceEngine)
        {
            this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll,
                                          "fetch-achannel-%s.log",
                                          params.producerId));
            isLoggerCreated_ = true;
        }
        
        int init(shared_ptr<Face> &face);
        int start();
        int stop();
        
    protected:
        virtual void onRTPPacketReceived(unsigned int len, unsigned char *data);
        virtual void onRTCPPacketReceived(unsigned int len, unsigned char *data);
        
    private:
        NdnAudioReceiver *audioReceiver_ = NULL;
    };
    
    class NdnAudioSendChannel : public NdnAudioChannel
    {
    public:
        NdnAudioSendChannel(const ParamsStruct &params,
                            webrtc::VoiceEngine *voiceEngine):
            NdnAudioChannel(params, voiceEngine)
        {
            this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll,
                                          "publish-achannel-%s.log",
                                          params.producerId));
            isLoggerCreated_ = true;
        }
        
        int init(shared_ptr<ndn::Transport> &transport);
        int start();
        int stop();
        
    protected:
        // webrtc::Transport interface
        int SendPacket(int channel, const void *data, int len);
        int SendRTCPPacket(int channel, const void *data, int len);
        
    private:
        NdnAudioSender *audioSender_ = NULL;
    };
}

#endif /* defined(__ndnrtc__ndnaudio_transport__) */
