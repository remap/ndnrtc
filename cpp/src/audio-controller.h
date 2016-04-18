//
//  audio-controller.h
//  libndnrtc
//
//  Copyright 2016 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __audio_controller_h__
#define __audio_controller_h__

#include "threading-capability.h"

namespace webrtc {
    class VoiceEngine;
}

namespace ndnrtc {
    class AudioController : public ThreadingCapability {
    public:
        static AudioController *getSharedInstance();
        
        webrtc::VoiceEngine *getVoiceEngine();
        void initVoiceEngine();
        void releaseVoiceEngine();
        void dispatchOnAudioThread(boost::function<void(void)> dispatchBlock);
        void performOnAudioThread(boost::function<void(void)> dispatchBlock);
        
        ~AudioController();
    private:
        AudioController();
        AudioController(AudioController const&) = delete;
        void operator=(AudioController const&) = delete;
        
        void initVE();
        
        webrtc::VoiceEngine *voiceEngine_ = NULL;
    };
}

#endif