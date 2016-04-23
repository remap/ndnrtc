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
    
    class AudioCapturerImpl;

    class AudioCapturer {
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
        getRtpNum();

        unsigned int
        getRtcpNum();

        static std::vector<std::pair<std::string, std::string>> getRecordingDevices();
        static std::vector<std::pair<std::string, std::string>> getPlayoutDevices();

    private:
        boost::shared_ptr<AudioCapturerImpl> pimpl_;
    };
}

#endif /* defined(__ndnrtc__audio_capturer__) */
