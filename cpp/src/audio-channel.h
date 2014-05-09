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
#include "audio-sender.h"
#include "av-sync.h"
#include "statistics.h"

namespace ndnrtc {
#if 0
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
        NdnAudioReceiveChannel(const ParamsStruct &params,
                               webrtc::VoiceEngine *voiceEngine);
        ~NdnAudioReceiveChannel();
        
        int init(shared_ptr<Face> &face);
        int start();
        int stop();
        
        void setAVSynchronizer(shared_ptr<AudioVideoSynchronizer> &avSync)
        {
            audioReceiver_->setAVSynchronizer(avSync);
        }
        void getStatistics(ReceiverChannelPerformance &stat);
        void setLogger(NdnLogger *logger)
        {
            LoggerObject::setLogger(logger);
            audioReceiver_->setLogger(logger);
        }
        
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
                            webrtc::VoiceEngine *voiceEngine);
        ~NdnAudioSendChannel();
        
        int init(const shared_ptr<Face> &face,
                 const shared_ptr<Transport> &transport);
        int start();
        int stop();
        
        void getStatistics(SenderChannelPerformance &stat);
        void setLogger(NdnLogger *logger)
        {
            LoggerObject::setLogger(logger);
            audioSender_->setLogger(logger);
        }
        
    protected:
        // webrtc::Transport interface
        int SendPacket(int channel, const void *data, int len);
        int SendRTCPPacket(int channel, const void *data, int len);
        
    private:
        NdnAudioSender *audioSender_ = NULL;
    };
#endif
}

#endif /* defined(__ndnrtc__ndnaudio_transport__) */
