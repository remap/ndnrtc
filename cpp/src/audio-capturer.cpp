//
//  audio-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include <webrtc/modules/audio_device/include/audio_device.h>
#include <webrtc/modules/audio_device/include/audio_device_defines.h>
#include <webrtc/voice_engine/include/voe_network.h>
#include <webrtc/voice_engine/include/voe_base.h>
#include <boost/thread/lock_guard.hpp>
#include <memory>

#include "audio-capturer.hpp"
#include "audio-controller.hpp"
#include "webrtc.hpp"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost;

namespace webrtc {
    class VoEHardware;
}

//******************************************************************************
namespace ndnrtc {
    class AudioCapturerImpl : public webrtc::Transport,
    public WebrtcAudioChannel, 
    public NdnRtcComponent
    {
    public:
        AudioCapturerImpl(const unsigned int deviceIdx, 
            IAudioSampleConsumer* sampleConsumer,
            const WebrtcAudioChannel::Codec& codec = WebrtcAudioChannel::Codec::G722);
        ~AudioCapturerImpl();

        void
        startCapture();

        void
        stopCapture();

        unsigned int
        getRtpNum() { return nRtp_; }

        unsigned int
        getRtcpNum() { return nRtcp_; }

    protected:
        friend AudioCapturer;

        boost::atomic<bool> capturing_;
        boost::mutex capturingState_;
        WebRtcSmartPtr<AudioDeviceModule> audioDeviceModule_;
        unsigned int nRtp_, nRtcp_;

        IAudioSampleConsumer* sampleConsumer_ = nullptr;

        bool
        SendRtp(const uint8_t* packet, size_t length, const PacketOptions& options);

        bool
        SendRtcp(const uint8_t* packet, size_t length);

    private:
        AudioCapturerImpl(const AudioCapturerImpl&) = delete;
    };
}

//******************************************************************************
//******************************************************************************
vector<pair<string,string>>
AudioCapturer::getRecordingDevices()
{
    vector<pair<string,string>> audioDevices;
    WebRtcSmartPtr<AudioDeviceModule> module(AudioDeviceModule::Create(0, AudioDeviceModule::kPlatformDefaultAudio));
    module->Init();
    
    int nDevices = module->RecordingDevices();
    for (int i = 0; i < nDevices; ++i) 
    {
        static char guid[128];
        static char name[128];
        memset(guid, 0, 128);
        memset(name, 0, 128);
        module->RecordingDeviceName(i, name, guid);

        audioDevices.push_back(pair<string,string>(string(guid), string(name)));
    }
    return audioDevices;
}

vector<pair<string,string>>
AudioCapturer::getPlayoutDevices()
{
    vector<pair<string,string>> audioDevices;
    WebRtcSmartPtr<AudioDeviceModule> module(AudioDeviceModule::Create(0, AudioDeviceModule::kPlatformDefaultAudio));
    module->Init();

    int nDevices = module->PlayoutDevices();
    for (int i = 0; i < nDevices; ++i) 
    {
        static char guid[128];
        static char name[128];
        memset(guid, 0, 128);
        memset(name, 0, 128);
        module->PlayoutDeviceName(i, name, guid);

        audioDevices.push_back(pair<string,string>(string(guid), string(name)));
    }
    return audioDevices;
}

//******************************************************************************
AudioCapturer::AudioCapturer(const unsigned int deviceIdx, 
            IAudioSampleConsumer* sampleConsumer,
            const WebrtcAudioChannel::Codec& codec):
pimpl_(std::make_shared<AudioCapturerImpl>(deviceIdx, sampleConsumer, codec))
{}

AudioCapturer::~AudioCapturer()
{
}

void
AudioCapturer::startCapture()
{
    pimpl_->startCapture();
}

void
AudioCapturer::stopCapture()
{
    pimpl_->stopCapture();
}

unsigned int
AudioCapturer::getRtpNum()
{
    return pimpl_->getRtpNum();
}

unsigned int 
AudioCapturer::getRtcpNum()
{
    return pimpl_->getRtcpNum();
}

void
AudioCapturer::setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger)
{
    pimpl_->setLogger(logger);    
}

//******************************************************************************
#pragma mark - construction/destruction
AudioCapturerImpl::AudioCapturerImpl(const unsigned int deviceIdx,
                IAudioSampleConsumer* sampleConsumer,
                const WebrtcAudioChannel::Codec& codec):
WebrtcAudioChannel(codec),
sampleConsumer_(sampleConsumer),
capturing_(false),
nRtp_(0), nRtcp_(0)
{
    assert(sampleConsumer);
    description_ = "audio-capturer";
    AudioController::getSharedInstance()->initVoiceEngine();
    int res = 0;

    audioDeviceModule_ = WebRtcSmartPtr<AudioDeviceModule>(AudioDeviceModule::Create(0, AudioDeviceModule::kPlatformDefaultAudio));
    audioDeviceModule_->Init();

    int nDevices = audioDeviceModule_->RecordingDevices();
    if (deviceIdx > nDevices) 
        res = 1;
    else
        audioDeviceModule_->SetRecordingDevice(deviceIdx);

    if (res != 0) 
        throw std::runtime_error("Can't initialize audio capturer");
}

AudioCapturerImpl::~AudioCapturerImpl()
{
    if (capturing_) stopCapture();
}

//******************************************************************************
#pragma mark - public
void AudioCapturerImpl::startCapture()
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

void AudioCapturerImpl::stopCapture()
{
    if (!capturing_) return;

    {
        boost::lock_guard<boost::mutex> scopedLock(capturingState_);
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
#if 0
int AudioCapturerImpl::SendPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        nRtp_++;
        AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
            sampleConsumer_->onDeliverRtpFrame(len, (uint8_t*)data);
        });
    }
    return len;
}

int AudioCapturerImpl::SendRTCPPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        nRtcp_++;
        AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
            sampleConsumer_->onDeliverRtcpFrame(len, (uint8_t*)data);
        });
    }
    return len;
}
#else
bool AudioCapturerImpl::SendRtp(const uint8_t* data, size_t len, const PacketOptions& options)
{
    boost::lock_guard<boost::mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        // as we are dispatching callback asynchronously, we have to
        // allocate memory for received data and wrap it in a shared_ptr
        nRtp_++;
        std::shared_ptr<AudioCapturerImpl> me = std::static_pointer_cast<AudioCapturerImpl>(shared_from_this());
        auto dataCopy = std::make_shared<std::vector<uint8_t>>(len);
        memcpy(dataCopy->data(), data, len);

        AudioController::getSharedInstance()->dispatchOnAudioThread([me, len, dataCopy](){
            me->sampleConsumer_->onDeliverRtpFrame(len, dataCopy->data());
        });
    }
    return true;
}

bool AudioCapturerImpl::SendRtcp(const uint8_t* data, size_t len)
{
    boost::lock_guard<boost::mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        nRtcp_++;
        std::shared_ptr<AudioCapturerImpl> me = std::static_pointer_cast<AudioCapturerImpl>(shared_from_this());
        auto dataCopy = std::make_shared<std::vector<uint8_t>>(len);
        memcpy(dataCopy->data(), data, len);

        AudioController::getSharedInstance()->dispatchOnAudioThread([me, len, dataCopy](){
            me->sampleConsumer_->onDeliverRtcpFrame(len, dataCopy->data());
        });
    }
    return true;
}
#endif
