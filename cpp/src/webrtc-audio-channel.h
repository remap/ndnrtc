//
//  webrtc-audio-channel.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__webrtc_audio_channel__
#define __ndnrtc__webrtc_audio_channel__

#include <webrtc/common_types.h>
#include <webrtc/voice_engine/include/voe_base.h>
#include <webrtc/voice_engine/include/voe_network.h>
#include <webrtc/voice_engine/include/voe_codec.h>

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
            
            virtual int
            release();

        protected:
            int webrtcChannelId_;
            webrtc::VoEBase* voeBase_;
            webrtc::VoENetwork* voeNetwork_;
            webrtc::VoECodec* voeCodec_;
            
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
