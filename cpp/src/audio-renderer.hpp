//
//  audio-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__audio_renderer__
#define __ndnrtc__audio_renderer__

#include "ndnrtc-object.hpp"
#include "webrtc-audio-channel.hpp"
#include "audio-capturer.hpp"

namespace webrtc {
    class VoEHardware;
}

namespace ndnrtc {
    class IAudioFrameConsumer {
    public:
        virtual void onDeliverRtpFrame(unsigned int len, unsigned char *data) = 0;
        virtual void onDeliverRtcpFrame(unsigned int len, unsigned char *data) = 0;
    };

    class AudioRenderer : public NdnRtcComponent,
                          public IAudioFrameConsumer,
                          public WebrtcAudioChannel,
                          public webrtc::Transport
    {
    public: 
        AudioRenderer(const unsigned int deviceIdx, 
            const WebrtcAudioChannel::Codec& codec);
        ~AudioRenderer();
        
        void startRendering();
        void stopRendering();
        
        // IAudioFrameConsumer interface conformance
        void onDeliverRtpFrame(unsigned int len, unsigned char *data);
        void onDeliverRtcpFrame(unsigned int len, unsigned char *data);
        
    private:
        AudioRenderer(const AudioRenderer&) = delete;
        
        boost::atomic<bool> rendering_;
        webrtc::VoEHardware* voeHardware_;
        WebrtcAudioChannel::Codec codec_;

        int
        SendPacket(int channel, const void *data, size_t len)
        { assert(false); }

        int
        SendRTCPPacket(int channel, const void *data, size_t len) 
        { return len; }
    };
}

#endif /* defined(__ndnrtc__audio_renderer__) */
