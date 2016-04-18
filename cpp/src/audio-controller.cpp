//
//  audio-controller.cpp
//  libndnrtc
//
//  Copyright 2016 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/voice_engine/include/voe_base.h>
#include <voice_engine/include/voe_audio_processing.h>

#include "audio-controller.h"
#include "simple-log.h"

using namespace ndnrtc;
using namespace webrtc;

AudioController::AudioController()
{
    startMyThread();
}

AudioController::~AudioController()
{
    stopMyThread();
}

AudioController* AudioController::getSharedInstance()
{
    static AudioController audioController;
    return &audioController;
}

webrtc::VoiceEngine *AudioController::getVoiceEngine()
{
    return voiceEngine_;
}

void AudioController::initVoiceEngine()
{
    dispatchOnMyThread([this](){
        initVE();
    });
}

void AudioController::releaseVoiceEngine()
{
    dispatchOnMyThread([this](){
        VoiceEngine::Delete(voiceEngine_);
    });
}

void AudioController::dispatchOnAudioThread(boost::function<void(void)> dispatchBlock)
{
    dispatchOnMyThread(dispatchBlock);
}

void AudioController::performOnAudioThread(boost::function<void(void)> dispatchBlock)
{
    performOnMyThread(dispatchBlock);
}

//******************************************************************************
void AudioController::initVE()
{
    if (voiceEngine_) return;

    voiceEngine_ = VoiceEngine::Create();
    
    int res = 0;
    
    {// init engine
        VoEBase *voe_base = VoEBase::GetInterface(voiceEngine_);
        
        res = voe_base->Init();
        
        voe_base->Release();
    }
    {// configure
        VoEAudioProcessing *voe_proc = VoEAudioProcessing::GetInterface(voiceEngine_);
        
        voe_proc->SetEcStatus(true, kEcConference);
        voe_proc->Release();
    }
}