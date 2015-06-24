//
//  webrtc-audio-channel.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__webrtc_audio_channel__
#define __ndnrtc__webrtc_audio_channel__

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "params.h"

namespace ndnrtc {
    namespace new_api {
        class WebrtcAudioChannel : public webrtc::Transport
        {
        public:
            WebrtcAudioChannel();
            virtual ~WebrtcAudioChannel();
            
            virtual int
            init();

        protected:
            int webrtcChannelId_;
            webrtc::VoEBase* voeBase_;
            webrtc::VoENetwork* voeNetwork_;
            
            // webrtc::Transport interface
            virtual int
            SendPacket(int channel, const void *data, size_t len)
            { return len; }
            
            virtual int
            SendRTCPPacket(int channel, const void *data, size_t len)
            { return len; }
        };
    }
}

#endif /* defined(__ndnrtc__webrtc_audio_channel__) */
