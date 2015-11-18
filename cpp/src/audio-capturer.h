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
#include "webrtc-audio-channel.h"

namespace ndnrtc {
    namespace new_api {
        
        class IAudioFrameConsumer
        {
        public:
            virtual void onDeliverRtpFrame(unsigned int len, unsigned char *data) = 0;
            virtual void onDeliverRtcpFrame(unsigned int len, unsigned char *data) = 0;
        };
        
        class AudioCapturer : public WebrtcAudioChannel, public NdnRtcComponent
        {
        public:
            AudioCapturer();
            ~AudioCapturer();
            
            void
            setFrameConsumer(IAudioFrameConsumer* frameConsumer)
            { frameConsumer_ = frameConsumer; }
            
            int
            startCapture();
            
            int
            stopCapture();
            
        protected:
            boost::atomic<bool> capturing_;
            IAudioFrameConsumer* frameConsumer_ = nullptr;
            
            int
            SendPacket(int channel, const void *data, size_t len);
            
            int
            SendRTCPPacket(int channel, const void *data, size_t len);
        };
    }
}

#endif /* defined(__ndnrtc__audio_capturer__) */
