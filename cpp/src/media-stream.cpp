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
    ThreadMap::iterator it = threads_.begin();
    
    while (it != threads_.end())
    {
        it->second->stop();
        it++;
    }
    
    threads_.clear();
}

int
MediaStream::init(const MediaStreamSettings& streamSettings)
{
    settings_ = streamSettings;
    streamName_ = settings_.streamParams_.streamName_;
    streamPrefix_ = *NdnRtcNamespace::getStreamPrefix(settings_.userPrefix_, streamName_);
    
    for (int i = 0; i < settings_.streamParams_.mediaThreads_.size(); i++)
        addNewMediaThread(settings_.streamParams_.mediaThreads_[i]);
    
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
    settings->streamPrefix_ = streamPrefix_;
    settings->certificateName_ = *NdnRtcNamespace::certificateNameForUser(settings_.userPrefix_);
    settings->keyChain_ = settings_.keyChain_;    
    settings->faceProcessor_ = settings_.faceProcessor_;
    settings->memoryCache_ = settings_.memoryCache_;
}

void
MediaStream::onMediaThreadRegistrationFailed(std::string threadName)
{
    LogWarnC << "failed to register thread " << threadName << std::endl;
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
    
    unsigned int tid;
    processThread_.Start(tid);
    
    return RESULT_OK;
}

MediaStreamParams
VideoStream::getStreamParameters()
{
    MediaStreamParams params(settings_.streamParams_);
    
    // update average segments numbers for frames
    for (int i = 0; i < params.mediaThreads_.size(); i++)
    {
        VideoThreadParams* threadParams = ((VideoThreadParams*)params.mediaThreads_[i]);
        std::string threadKey = *NdnRtcNamespace::buildPath(false,
                                                            &streamPrefix_,
                                                            &threadParams->threadName_,
                                                            0);
        VideoThreadStatistics stat;
        shared_ptr<VideoThread> thread = dynamic_pointer_cast<VideoThread>(threads_[threadKey]);
        
        thread->getStatistics(stat);
        threadParams->deltaAvgSegNum_ = stat.deltaAvgSegNum_;
        threadParams->deltaAvgParitySegNum_ = stat.deltaAvgParitySegNum_;
        threadParams->keyAvgSegNum_ = stat.keyAvgSegNum_;
        threadParams->keyAvgParitySegNum_ = stat.keyAvgParitySegNum_;
    }
    
    return params;
}

void
VideoStream::release()
{
    processThread_.SetNotAlive();
    deliverEvent_.Set();
    processThread_.Stop();
}

void
VideoStream::addNewMediaThread(const MediaThreadParams* params)
{
    VideoThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    
    threadSettings.useFec_ = settings_.useFec_;
    threadSettings.threadParams_ = params;
    
    shared_ptr<VideoThread> videoThread(new VideoThread());
    videoThread->registerCallback(this);
    videoThread->setLogger(logger_);
    
    if (RESULT_FAIL(videoThread->init(threadSettings)))
        notifyError(-1, "couldn't add new video thread %s", threadSettings.getVideoParams()->threadName_.c_str());
    else
        threads_[videoThread->getPrefix()] = videoThread;
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
    audioCapturer_->registerCallback(this);
    audioCapturer_->setFrameConsumer(this);
}

int
AudioStream::init(const MediaStreamSettings& streamSettings)
{
    MediaStream::init(streamSettings);
    audioCapturer_->setLogger(logger_);
    audioCapturer_->init();
    audioCapturer_->startCapture();
    
    return RESULT_OK;
}

void
AudioStream::release()
{
    audioCapturer_->stopCapture();
}

void
AudioStream::addNewMediaThread(const MediaThreadParams* params)
{
    AudioThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    threadSettings.threadParams_ = params;
    
    shared_ptr<AudioThread> audioThread(new AudioThread());
    audioThread->registerCallback(this);
    audioThread->setLogger(logger_);
    
    if (RESULT_FAIL(audioThread->init(threadSettings)))
        notifyError(-1, "couldn't add new audio thread %s",
                    threadSettings.threadParams_->threadName_.c_str());
    else
        threads_[audioThread->getPrefix()] = audioThread;
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
