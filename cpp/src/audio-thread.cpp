//
//  audio-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "audio-thread.h"

using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
#pragma mark - public
AudioThread::AudioThread(const AudioThreadParams& params,
    const AudioCaptureParams& captureParams,
    IAudioThreadCallback* callback,
    size_t bundleWireLength):
callback_(callback),
bundle_(bundleWireLength),
capturer_(captureParams.deviceId_, this, 
    (params.codec_ == "opus" ? WebrtcAudioChannel::Codec::Opus : WebrtcAudioChannel::Codec::G722)),
isRunning_(false)
{
    description_ = "athread";
}

AudioThread::~AudioThread()
{
    if (isRunning_) stop();
}

void AudioThread::start()
{
    if (isRunning_) throw std::runtime_error("Audio thread already started");
    isRunning_ = true;
    capturer_.startCapture();
}

void AudioThread::stop()
{
    if (isRunning_) capturer_.stopCapture();
    isRunning_ = false;
}

//******************************************************************************
void AudioThread::onDeliverRtpFrame(unsigned int len, uint8_t* data)
{   
    AudioBundlePacket::AudioSampleBlob blob({false}, len, data);
    deliver(blob);
}

void AudioThread::onDeliverRtcpFrame(unsigned int len, uint8_t* data)
{
    AudioBundlePacket::AudioSampleBlob blob({true}, len, data);
    deliver(blob);
}

void AudioThread::deliver(const AudioBundlePacket::AudioSampleBlob& blob)
{
    if (!bundle_.hasSpace(blob))
    {
        callback_->onSampleBundle(bundle_);
        bundle_.clear();
    }

    bundle_ << blob;
}
