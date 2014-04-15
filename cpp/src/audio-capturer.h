//
//  audio-capturer.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__audio_capturer__
#define __ndnrtc__audio_capturer__

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "statistics.h"
#include "webrtc-audio-channel.h"

namespace ndnrtc {
    namespace new_api {
        
        class IAudioFrameConsumer
        {
        public:
            virtual void onDeliverRtpFrame(unsigned int len, unsigned char *data) = 0;
            virtual void onDeliverRtcpFrame(unsigned int len, unsigned char *data) = 0;
        };
        
        class AudioCapturer : public WebrtcAudioChannel, public NdnRtcObject
        {
        public:
            AudioCapturer(const ParamsStruct& params,
                          webrtc::VoiceEngine* voiceEngine);
            ~AudioCapturer();
            
            void
            setFrameConsumer(IAudioFrameConsumer* frameConsumer)
            { frameConsumer_ = frameConsumer; }
            
            int
            startCapture();
            
            int
            stopCapture();
            
            // statistics
            void
            getStatistics(SenderChannelPerformance& stat);
            
        protected:
            IAudioFrameConsumer* frameConsumer_ = nullptr;
            
            int
            SendPacket(int channel, const void *data, int len);
            
            int
            SendRTCPPacket(int channel, const void *data, int len);
        };
    }
}

#endif /* defined(__ndnrtc__audio_capturer__) */
