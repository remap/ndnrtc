//
//  audio-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include <webrtc/voice_engine/include/voe_hardware.h>
#include <webrtc/voice_engine/include/voe_network.h>
#include <webrtc/voice_engine/include/voe_base.h>
#include <boost/thread/lock_guard.hpp>

#include "audio-capturer.h"
#include "audio-controller.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost;

//******************************************************************************
vector<pair<string,string>>
AudioCapturer::getRecordingDevices()
{
    vector<pair<string,string>> audioDevices;
    AudioController::getSharedInstance()->initVoiceEngine();
    AudioController::getSharedInstance()->performOnAudioThread([&audioDevices](){
        VoEHardware* hardware = VoEHardware::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());
        int nDevices = 0;

        hardware->GetNumOfRecordingDevices(nDevices);
        for (int i = 0; i < nDevices; ++i) 
        {
            static char guid[128];
            static char name[128];
            memset(guid, 0, 128);
            memset(name, 0, 128);
            hardware->GetRecordingDeviceName(i, guid, name);

            audioDevices.push_back(pair<string,string>(string(guid), string(name)));
        }
        hardware->Release();
    });
    return audioDevices;
}

vector<pair<string,string>>
AudioCapturer::getPlayoutDevices()
{
    vector<pair<string,string>> audioDevices;
    AudioController::getSharedInstance()->initVoiceEngine();
    AudioController::getSharedInstance()->performOnAudioThread([&audioDevices](){
        VoEHardware* hardware = VoEHardware::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());
        int nDevices = 0;

        hardware->GetNumOfPlayoutDevices(nDevices);
        for (int i = 0; i < nDevices; ++i) 
        {
            static char guid[128];
            static char name[128];
            memset(guid, 0, 128);
            memset(name, 0, 128);
            hardware->GetPlayoutDeviceName(i, guid, name);

            audioDevices.push_back(pair<string,string>(string(guid), string(name)));
        }
        hardware->Release();
    });
    return audioDevices;
}

//******************************************************************************
#pragma mark - construction/destruction
AudioCapturer::AudioCapturer(const unsigned int deviceIdx,
                IAudioSampleConsumer* sampleConsumer,
                const WebrtcAudioChannel::Codec& codec):
WebrtcAudioChannel(codec),
sampleConsumer_(sampleConsumer),
capturing_(false)
{
    assert(sampleConsumer);
    description_ = "audio-capturer";
    AudioController::getSharedInstance()->initVoiceEngine();
    int res = 0;
    AudioController::getSharedInstance()->performOnAudioThread([this, deviceIdx, &res](){
        voeHardware_ = VoEHardware::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());
        int nDevices = 0;
        voeHardware_->GetNumOfRecordingDevices(nDevices);
        if (deviceIdx > nDevices) 
        {
            res = 1;
            voeHardware_->Release();
        }
        else
            voeHardware_->SetRecordingDevice(deviceIdx);
    });

    if (res != 0) 
        throw std::runtime_error("Can't initialize audio capturer");
}

AudioCapturer::~AudioCapturer()
{
    if (capturing_) stopCapture();
    voeHardware_->Release();
}

//******************************************************************************
#pragma mark - public
void AudioCapturer::startCapture()
{
    if (capturing_) 
        throw std::runtime_error("Audio capturing is already initiated");

    int res = RESULT_OK;
    
    AudioController::getSharedInstance()->performOnAudioThread([this, &res](){
        if (voeNetwork_->RegisterExternalTransport(webrtcChannelId_, *this) < 0) 
            res = voeBase_->LastError();
        if (voeBase_->StartSend(webrtcChannelId_) < 0) 
            res = voeBase_->LastError();
    });
    
    if (RESULT_GOOD(res))
    {
        capturing_ = true;
        LogInfoC << "started" << endl;
    }
    else
    {
        stringstream ss;
        ss << "Can't start capturing due to WebRTC error " << res;
        throw std::runtime_error(ss.str());
    }
}

void AudioCapturer::stopCapture()
{
    if (!capturing_) return;

    {
        boost::lock_guard<mutex> scopedLock(capturingState_);
        capturing_ = false;
    }

    AudioController::getSharedInstance()->performOnAudioThread([this](){
        voeBase_->StopSend(webrtcChannelId_);
        voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
    });

    LogInfoC << "stopped" << endl;
}

//******************************************************************************
#pragma mark - private
int AudioCapturer::SendPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
            sampleConsumer_->onDeliverRtpFrame(len, (uint8_t*)data);
        });
    }
    return len;
}

int AudioCapturer::SendRTCPPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
            sampleConsumer_->onDeliverRtcpFrame(len, (uint8_t*)data);
        });
    }
    return len;
}
