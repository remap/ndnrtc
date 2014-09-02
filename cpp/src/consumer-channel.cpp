//
//  consumer-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "consumer-channel.h"
#include "rtt-estimation.h"
#include "ndnrtc-namespace.h"
#include "av-sync.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog::new_api;
using namespace boost;

//******************************************************************************
#pragma mark - construction/destruction
ConsumerChannel::ConsumerChannel(const ParamsStruct& params,
                                 const ParamsStruct& audioParams,
                                 IExternalRenderer* const videoRenderer,
                                 const shared_ptr<FaceProcessor>& faceProcessor):
ndnrtc::NdnRtcObject(params),
isOwnFace_(false),
audioParams_(audioParams),
rttEstimation_(new RttEstimation()),
faceProcessor_(faceProcessor)
{
    rttEstimation_->setDescription(NdnRtcUtils::toString("%s-rtt-est", getDescription().c_str()));
    
    if (!faceProcessor_.get())
    {
        isOwnFace_ = true;
        faceProcessor_ = FaceProcessor::createFaceProcessor(params_,
                                                            NdnRtcNamespace::defaultKeyChain());
    }
    
    if (params.useVideo)
    {
        videoInterestQueue_.reset(new InterestQueue(faceProcessor_->getFaceWrapper()));
        videoInterestQueue_->setDescription(std::string("video-iqueue"));
        videoConsumer_.reset(new VideoConsumer(params_, videoInterestQueue_,
                                               rttEstimation_, videoRenderer));
    }
    
    if (params_.useAudio)
    {
        audioInterestQueue_.reset(new InterestQueue(faceProcessor_->getFaceWrapper()));
        audioInterestQueue_->setDescription(std::string("audio-iqueue"));
        audioConsumer_.reset(new AudioConsumer(audioParams_, audioInterestQueue_, rttEstimation_));
    }
    
#ifdef SERVICE_CHANNEL
    serviceChannel_.reset(new ServiceChannel(this, faceProcessor_));
#endif
    this->setLogger(new Logger(params_.loggingLevel,
                               NdnRtcUtils::toString("consumer-%s.log",
                                                     params.producerId)));
    isLoggerCreated_ = true;
    
    if (params.useAvSync && params.useAudio && params.useVideo)
    {
#ifdef AV_SYNC_AUDIO_MASTER
        shared_ptr<AudioVideoSynchronizer> avSync(new AudioVideoSynchronizer(audioConsumer_, videoConsumer_));
#else
        shared_ptr<AudioVideoSynchronizer> avSync(new AudioVideoSynchronizer(videoConsumer_, audioConsumer_));
#endif
        avSync->setLogger(logger_);
        
        audioConsumer_->setAvSynchronizer(avSync);
        videoConsumer_->setAvSynchronizer(avSync);
    }

    isLoggerCreated_ = true;
}

//******************************************************************************
#pragma mark - public
int
ConsumerChannel::init()
{
#warning handle errors
    if (params_.useAudio)
        audioConsumer_->init();

    if (params_.useVideo)
        videoConsumer_->init();
    
    return RESULT_OK;
}

int
ConsumerChannel::startTransmission()
{
#warning handle errors
#ifdef SERVICE_CHANNEL
    serviceChannel_->startMonitor(*NdnRtcNamespace::getSessionInfoPrefix(params_));
#endif
    if (isOwnFace_)
        faceProcessor_->startProcessing();

    if (params_.useAudio)
        audioConsumer_->start();
    
    if (params_.useVideo)
        videoConsumer_->start();
    
    return RESULT_OK;
}

int
ConsumerChannel::stopTransmission()
{
#warning handle errors
#ifdef SERVICE_CHANNEL
    serviceChannel_->stopMonitor();
#endif
    
#ifndef AUDIO_OFF
    if (params_.useAudio)
        audioConsumer_->stop();
#endif
    if (params_.useVideo)
        videoConsumer_->stop();
    
    if (isOwnFace_)
        faceProcessor_->stopProcessing();
    
    return RESULT_OK;
}

void
ConsumerChannel::getChannelStatistics(ReceiverChannelStatistics &stat)
{   
    if (params_.useVideo)
        videoConsumer_->getStatistics(stat.videoStat_);
    
#ifndef AUDIO_OFF
    if (params_.useAudio)
        audioConsumer_->getStatistics(stat.audioStat_);
#endif
}

void
ConsumerChannel::setLogger(ndnlog::new_api::Logger *logger)
{
#ifndef AUDIO_OFF
    if (params_.useAudio)
        audioConsumer_->setLogger(logger);
#endif
    if (params_.useVideo)
        videoConsumer_->setLogger(logger);
    
    rttEstimation_->setLogger(logger);
    faceProcessor_->setLogger(logger);

    if (videoInterestQueue_.get())
        videoInterestQueue_->setLogger(logger);
    
    if (audioInterestQueue_.get())
        audioInterestQueue_->setLogger(logger);
    
#ifdef SERVICE_CHANNEL
    serviceChannel_->setLogger(logger);
#endif
    ILoggingObject::setLogger(logger);
}

//******************************************************************************
#pragma mark - private
void
ConsumerChannel::onInterest(const shared_ptr<const Name>& prefix,
                        const shared_ptr<const Interest>& interest,
                        ndn::Transport& transport)
{
    // empty
}

void
ConsumerChannel::onRegisterFailed(const shared_ptr<const Name>&
                              prefix)
{
    // empty
}

void
ConsumerChannel::onProducerParametersUpdated(const ParamsStruct& newVideoParams,
                            const ParamsStruct& newAudioParams)
{
    std::stringstream ss;
    
    ss << "vstreams " << newVideoParams.nStreams << " ";
    for (int i = 0; i < newVideoParams.nStreams; i++)
        ss << "[" << i
        << "| " << newVideoParams.streamsParams[i].codecFrameRate
        << " " << newVideoParams.streamsParams[i].gop
        << " " << newVideoParams.streamsParams[i].startBitrate
        << " " << newVideoParams.streamsParams[i].encodeWidth
        << " " << newVideoParams.streamsParams[i].encodeHeight
        << "] ";

    ss << "astreams " << newAudioParams.nStreams;
    for (int i = 0; i < newAudioParams.nStreams; i++)
        ss << "[" << i
        << "| " << newAudioParams.streamsParams[i].codecFrameRate
        << " " << newAudioParams.streamsParams[i].startBitrate
        << "] ";
    
    LogInfoC << "producer parameters updated: " << ss.str() << std::endl;
    
}

void
ConsumerChannel::onUpdateFailedWithTimeout()
{
#ifdef SERVICE_CHANNEL
    LogWarnC << "service update failed with timeout" << std::endl;
    serviceChannel_->startMonitor(*NdnRtcNamespace::getSessionInfoPrefix(params_));
#endif
}

void
ConsumerChannel::onUpdateFailedWithError(const char* errMsg)
{
#ifdef SERVICE_CHANNEL
    LogErrorC << "service update failed with message: "
    << std::string(errMsg) << std::endl;
#endif
}

