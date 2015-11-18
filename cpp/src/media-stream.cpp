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
#include "ndnrtc-namespace.h"

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
    streamPrefix_ = *NdnRtcNamespace::getStreamPrefix(settings_.userPrefix_, streamName_);
    
    int res = RESULT_OK;
    for (int i = 0; i < settings_.streamParams_.mediaThreads_.size() && RESULT_GOOD(res); i++)
        res = addNewMediaThread(settings_.streamParams_.mediaThreads_[i]);
    
    return res;
}

void
MediaStream::release()
{
    ThreadMap::iterator it = threads_.begin();
    while (it != threads_.end())
    {
        it->second->stop();
        it++;
    }
    
    threads_.clear();
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
capturer_(new ExternalCapturer())
{
    description_ = "video-stream";
    capturer_->setFrameConsumer(this);
}

int
VideoStream::init(const MediaStreamSettings& streamSettings)
{
    if (RESULT_GOOD(MediaStream::init(streamSettings)))
    {
        encodingBarrier_ = new boost::barrier(threads_.size());
        capturer_->setLogger(logger_);
        isProcessing_ = true;
        
        return RESULT_OK;
    }
    return RESULT_ERR;
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

bool
VideoStream::isStreamStatReady()
{
    bool statReady = true;
    
    for (auto it:threads_)
        statReady &= dynamic_pointer_cast<VideoThread>(it.second)->isThreadStatReady();
    
    return statReady;
}

void
VideoStream::release()
{
    MediaStream::release();
    
    if (isProcessing_)
    {
        delete encodingBarrier_;
        isProcessing_ = false;
    }
}

int
VideoStream::addNewMediaThread(const MediaThreadParams* params)
{
    VideoThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    
    threadSettings.useFec_ = settings_.useFec_;
    threadSettings.threadParams_ = params;
    threadSettings.threadCallback_ = this;
    
    shared_ptr<VideoThread> videoThread(new VideoThread());
    videoThread->registerCallback(this);
    videoThread->setLogger(logger_);
    
    if (RESULT_FAIL(videoThread->init(threadSettings)))
    {
        notifyError(-1, "couldn't add new video thread %s", threadSettings.getVideoParams()->threadName_.c_str());
        return RESULT_ERR;
    }
    else
    {
        threads_[videoThread->getPrefix()] = videoThread;
        deltaFrameSync_[params->threadName_] = 0;
        keyFrameSync_[params->threadName_] = 0;
    }

    return RESULT_OK;
}

void
VideoStream::onDeliverFrame(WebRtcVideoFrame &frame,
                            double timestamp)
{
    if (isProcessing_)
    {   
        if (!frame.IsZeroSize()) {
            for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
            {
                VideoThreadSettings set;
                ((VideoThread*)i->second.get())->getSettings(set);
                
                LogTraceC << "delivering frame to " << set.getVideoParams()->threadName_ << std::endl;
                ((VideoThread*)i->second.get())->onDeliverFrame(frame, timestamp);
            }
        }
    }
}

void
VideoStream::onFrameDropped(const std::string& threadName)
{
    encodingBarrier_->wait();
}

void
VideoStream::onFrameEncoded(const std::string& threadName,
                            const FrameNumber& frameNo,
                            bool isKey)
{
    if (isKey)
        keyFrameSync_[threadName] = frameNo;
    else
        deltaFrameSync_[threadName] = frameNo;
    
    encodingBarrier_->wait();
    
#ifdef NDN_TRACE
    std::stringstream ss1, ss2;
    ss1 << "K: ";
    ss2 << "D: ";
    
    for (auto it:keyFrameSync_) ss1 << it.first << "-" << it.second << " ";
    for (auto it:deltaFrameSync_) ss2 << it.first << "-" << it.second << " ";
        
    LogDebugC << "thread sync list: " << ss1.str() << ss2.str() << std::endl;
#endif
}

ThreadSyncList
VideoStream::getFrameSyncList(bool isKey)
{
    ThreadSyncList sl;
    std::map<std::string, PacketNumber> &mapList = (isKey)?keyFrameSync_:deltaFrameSync_;
    
    copy(mapList.begin(), mapList.end(), std::back_inserter(sl));
    
    return sl;
}

//******************************************************************************
AudioStream::AudioStream():
MediaStream(),
audioCapturer_(new AudioCapturer())
{
    description_ = "audio-stream";
    audioCapturer_->registerCallback(this);
    audioCapturer_->setFrameConsumer(this);
}

AudioStream::~AudioStream()
{
//    audioCapturer_->stopCapture();
}

int
AudioStream::init(const MediaStreamSettings& streamSettings)
{
    if (RESULT_GOOD(MediaStream::init(streamSettings)))
    {
        isProcessing_ = true;
        audioCapturer_->setLogger(logger_);
        audioCapturer_->startCapture();
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void
AudioStream::release()
{
    audioCapturer_->stopCapture();
    MediaStream::release();
    isProcessing_ = false;
}

int
AudioStream::addNewMediaThread(const MediaThreadParams* params)
{
    AudioThreadSettings threadSettings;
    getCommonThreadSettings(&threadSettings);
    threadSettings.threadParams_ = params;
    
    shared_ptr<AudioThread> audioThread(new AudioThread());
    audioThread->registerCallback(this);
    audioThread->setLogger(logger_);
    
    if (RESULT_FAIL(audioThread->init(threadSettings)))
    {
        notifyError(-1, "couldn't add new audio thread %s",
                    threadSettings.threadParams_->threadName_.c_str());
        return RESULT_ERR;
    }
    else
        threads_[audioThread->getPrefix()] = audioThread;
    
    return RESULT_OK;
}

// interface conformance - IAudioFrameConsumer
void
AudioStream::onDeliverRtpFrame(unsigned int len, unsigned char *data)
{
    if (isProcessing_)
        for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
            ((AudioThread*)i->second.get())->onDeliverRtpFrame(len, data);
}

void
AudioStream::onDeliverRtcpFrame(unsigned int len, unsigned char *data)
{
    if (isProcessing_)
        for (ThreadMap::iterator i = threads_.begin(); i != threads_.end(); i++)
            ((AudioThread*)i->second.get())->onDeliverRtcpFrame(len, data);
}
