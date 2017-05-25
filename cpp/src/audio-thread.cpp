//
//  audio-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <boost/make_shared.hpp>
#include <ndn-cpp/data.hpp>

#include "audio-thread.hpp"
#include "estimators.hpp"
#include "frame-data.hpp"

using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
#pragma mark - public
AudioThread::AudioThread(const AudioThreadParams& params,
    const AudioCaptureParams& captureParams,
    IAudioThreadCallback* callback,
    size_t bundleWireLength):
bundleNo_(0),
rateMeter_(boost::make_shared<estimators::TimeWindow>(250)),
threadName_(params.threadName_),
codec_(params.codec_),
callback_(callback),
bundle_(boost::make_shared<AudioBundlePacket>(bundleWireLength)),
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
    bundleNo_ = 0;
    capturer_.startCapture();

    LogDebugC << "started" << std::endl;
}

void AudioThread::stop()
{
    if (isRunning_) 
    {
        isRunning_ = false;
        capturer_.stopCapture();
        LogDebugC << "stopped" << std::endl;
    }
}

double AudioThread::getRate() const
{
    return rateMeter_.value();
}

void AudioThread::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    ILoggingObject::setLogger(logger);
    capturer_.setLogger(logger);
}

//******************************************************************************
void AudioThread::onDeliverRtpFrame(unsigned int len, uint8_t* data)
{   
    if (isRunning_)
    {
        LogTraceC << "delivering rtp frame" << std::endl;
        std::vector<uint8_t> adata(data, data+len);
        AudioBundlePacket::AudioSampleBlob blob({false}, adata.begin(), adata.end());
        deliver(blob);
    }
}

void AudioThread::onDeliverRtcpFrame(unsigned int len, uint8_t* data)
{
    if (isRunning_)
    {
        LogTraceC << "delivering rtcp frame" << std::endl;
        std::vector<uint8_t> adata(data, data+len);
        AudioBundlePacket::AudioSampleBlob blob({true}, adata.begin(), adata.end());
        deliver(blob);
    }
}

void AudioThread::deliver(const AudioBundlePacket::AudioSampleBlob& blob)
{
    if (!bundle_->hasSpace(blob))
    {
        rateMeter_.newValue(0);
        callback_->onSampleBundle(threadName_, bundleNo_++, bundle_);
        bundle_->clear();
    }

    *bundle_ << blob;
}
