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

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "params.h"
#include "webrtc-audio-channel.h"
#include "audio-capturer.h"
#include "renderer.h"

namespace ndnrtc {
    namespace new_api {
        class AudioRenderer : public IRenderer, public IAudioFrameConsumer,
        public NdnRtcObject,
        public WebrtcAudioChannel
        {
        public: 
            AudioRenderer(const ParamsStruct& params);
            ~AudioRenderer();
            
            int
            init();
            
            int
            startRendering(const std::string &name = "AudioRenderer");
            
            int
            stopRendering();
            
            // IAudioFrameConsumer interface conformance
            void onDeliverRtpFrame(unsigned int len, unsigned char *data);
            void onDeliverRtcpFrame(unsigned int len, unsigned char *data);
            
        protected:
            
        };
    }
}

#endif /* defined(__ndnrtc__audio_renderer__) */
