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
#include <boost/make_shared.hpp>

#include "audio-capturer.h"
#include "audio-controller.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost;

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
        webrtc::VoEHardware* voeHardware_;
        unsigned int nRtp_, nRtcp_;

        IAudioSampleConsumer* sampleConsumer_ = nullptr;

        int
        SendPacket(int channel, const void *data, size_t len);

        int
        SendRTCPPacket(int channel, const void *data, size_t len);

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
AudioCapturer::AudioCapturer(const unsigned int deviceIdx, 
            IAudioSampleConsumer* sampleConsumer,
            const WebrtcAudioChannel::Codec& codec):
pimpl_(boost::make_shared<AudioCapturerImpl>(deviceIdx, sampleConsumer, codec))
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

AudioCapturerImpl::~AudioCapturerImpl()
{
    if (capturing_) stopCapture();
    voeHardware_->Release();
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

// NOTE: should be called on audio thread
void AudioCapturerImpl::stopCapture()
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
#if 0
int AudioCapturerImpl::SendPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        nRtp_++;
        std::cout << "try deliver rtp" << std::endl;
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
        std::cout << "try deliver rtcp" << std::endl;
        AudioController::getSharedInstance()->performOnAudioThread([this, len, data](){
            sampleConsumer_->onDeliverRtcpFrame(len, (uint8_t*)data);
        });
    }
    return len;
}
#else
int AudioCapturerImpl::SendPacket(int channel, const void *data, size_t len)
{
    assert(webrtcChannelId_ == channel);
    boost::lock_guard<mutex> scopedLock(capturingState_);

    if (capturing_)
    {
        // as we are dispatching callback asynchronously, we have to
        // allocate memory for received data and wrap it in a shared_ptr
        nRtp_++;
        boost::shared_ptr<AudioCapturerImpl> me = boost::static_pointer_cast<AudioCapturerImpl>(shared_from_this());
        boost::shared_ptr<uint8_t[]> dataCopy(new uint8_t[len]);
        memcpy(dataCopy.get(), data, len);

        AudioController::getSharedInstance()->dispatchOnAudioThread([me, len, dataCopy](){
            me->sampleConsumer_->onDeliverRtpFrame(len, dataCopy.get());
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
        boost::shared_ptr<AudioCapturerImpl> me = boost::static_pointer_cast<AudioCapturerImpl>(shared_from_this());
        boost::shared_ptr<uint8_t[]> dataCopy(new uint8_t[len]);
        memcpy(dataCopy.get(), data, len);

        AudioController::getSharedInstance()->dispatchOnAudioThread([me, len, dataCopy](){
            me->sampleConsumer_->onDeliverRtcpFrame(len, dataCopy.get());
        });
    }
    return len;
}
#endif
