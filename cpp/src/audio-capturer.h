//
//  audio-capturer.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__audio_capturer__
#define __ndnrtc__audio_capturer__

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <webrtc/voice_engine/include/voe_base.h>

#include "webrtc-audio-channel.h"
#include "ndnrtc-object.h"

namespace webrtc {
    class VoEHardware;
}

namespace ndnrtc {
    class IAudioSampleConsumer
    {
    public:
        virtual void onDeliverRtpFrame(unsigned int len, uint8_t* data) = 0;
        virtual void onDeliverRtcpFrame(unsigned int len, uint8_t* data) = 0;
    };
    
    class AudioCapturer : public webrtc::Transport,
    public WebrtcAudioChannel, 
    public NdnRtcComponent
    {
    public:
        AudioCapturer(const unsigned int deviceIdx, 
            IAudioSampleConsumer* sampleConsumer,
            const WebrtcAudioChannel::Codec& codec = WebrtcAudioChannel::Codec::G722);
        ~AudioCapturer();
        
        void
        startCapture();
        
        void
        stopCapture();

        unsigned int
        getRtpNum() { return nRtp_; }

        unsigned int
        getRtcpNum() { return nRtcp_; }

        static std::vector<std::pair<std::string, std::string>> getRecordingDevices();
        static std::vector<std::pair<std::string, std::string>> getPlayoutDevices();

    protected:
        boost::atomic<bool> capturing_;
        boost::mutex capturingState_;
        webrtc::VoEHardware* voeHardware_;
        unsigned int nRtp_, nRtcp_;
        
        IAudioSampleConsumer* sampleConsumer_ = nullptr;
        
        int
        SendPacket(int channel, const void *data, size_t len);
        
        int
        SendRTCPPacket(int channel, const void *data, size_t len);

    private:
        AudioCapturer(const AudioCapturer&) = delete;
    };
}

#endif /* defined(__ndnrtc__audio_capturer__) */
