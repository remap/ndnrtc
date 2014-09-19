//
//  media-stream.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "media-stream.h"
#include "video-thread.h"
#include "audio-thread.h"

using namespace webrtc;
using namespace boost;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
MediaStream::MediaStream()
{
}

MediaStream::~MediaStream()
{
}

int
MediaStream::init(const MediaStreamSettings& streamSettings)
{
    settings_ = streamSettings;
    streamName_ = settings_.streamParams_.streamName_;
    streamPrefix_ = Name(*NdnRtcNamespace::getStreamPrefix(settings_.userPrefix_, streamName_));
    
    for (int i = 0; i < settings_.streamParams_.mediaThreads_.size(); i++)
    {
        addNewMediaThread(settings_.streamParams_.mediaThreads_[i]);
    }
    
    return RESULT_OK;
}

void
MediaStream::addThread(const MediaThreadParams& threadParams)
{
    
}

void
MediaStream::removeThread(const std::string& threadName)
{
    
}

void
MediaStream::removeAllThreads()
{
    
}

// private
void
MediaStream::getCommonThreadSettings(MediaThreadSettings* settings)
{
    settings->segmentSize_ = settings_.streamParams_.producerParams_.segmentSize_;
    settings->dataFreshnessMs_ = settings_.streamParams_.producerParams_.freshnessMs_;
    settings->streamPrefix_ = streamPrefix_.toUri();
    settings->certificateName_ = *NdnRtcNamespace::certificateNameForUser(settings_.userPrefix_);
}

//******************************************************************************
VideoStream::VideoStream():
MediaStream(),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processFrameDelivery, this,
                                            kHighPriority))

{
    description_ = "video-stream";
}

int
VideoStream::init(const MediaStreamSettings& streamSettings)
{
    MediaStream::init(streamSettings);
    
    capturer_.reset(new ExternalCapturer());
    capturer_->setFrameConsumer(this);
    
    return RESULT_OK;
}

void
VideoStream::addNewMediaThread(const MediaThreadParams* params)
{
    VideoThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    
    threadSettings.useFec_ = settings_.useFec_;
    threadSettings.threadParams_ = *params;
    
    shared_ptr<VideoThread> videoThread(new VideoThread());
    videoThread->init(threadSettings);
    
    threads_[videoThread->getPrefix()] = videoThread;
    videoThread->setLogger(logger_);
}

void
VideoStream::onDeliverFrame(webrtc::I420VideoFrame &frame,
                                 double timestamp)
{
    deliver_cs_->Enter();
    deliverFrame_.SwapFrame(&frame);
    deliveredTimestamp_ = timestamp;
    deliver_cs_->Leave();
    
    deliverEvent_.Set();
};

bool
VideoStream::processDeliveredFrame()
{
    if (deliverEvent_.Wait(100) == kEventSignaled)
    {
        deliver_cs_->Enter();
        if (!deliverFrame_.IsZeroSize()) {
            for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
                ((VideoThread*)i->second.get())->onDeliverFrame(deliverFrame_, deliveredTimestamp_);
        }
        deliver_cs_->Leave();
    }
    
    return true;
}

//******************************************************************************
AudioStream::AudioStream():
MediaStream(),
audioCapturer_(new AudioCapturer(NdnRtcUtils::sharedVoiceEngine()))
{
    description_ = "audio-stream";
    audioCapturer_->setFrameConsumer(this);
}

int
AudioStream::init(const MediaStreamSettings& streamSettings)
{
    MediaStream::init(streamSettings);
    audioCapturer_->startCapture();
    
    return RESULT_OK;
}

void
AudioStream::addNewMediaThread(const MediaThreadParams* params)
{
    AudioThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    
    shared_ptr<AudioThread> audioThread(new AudioThread());
    audioThread->init(threadSettings);
    
    threads_[audioThread->getPrefix()] = audioThread;
    audioThread->setLogger(logger_);
}

// interface conformance - IAudioFrameConsumer
void
AudioStream::onDeliverRtpFrame(unsigned int len, unsigned char *data)
{
    for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
        ((AudioThread*)i->second.get())->onDeliverRtpFrame(len, data);
}

void
AudioStream::onDeliverRtcpFrame(unsigned int len, unsigned char *data)
{
    for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
        ((AudioThread*)i->second.get())->onDeliverRtcpFrame(len, data);
}
