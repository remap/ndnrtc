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
using namespace std;

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
        faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
        faceProcessor_->getFaceWrapper()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                                  bind(&ConsumerChannel::onInterest,
                                                       this, _1, _2, _3),
                                                  bind(&ConsumerChannel::onRegisterFailed,
                                                       this, _1));
        faceProcessor_->setDescription(NdnRtcUtils::toString("%s-faceproc", getDescription().c_str()));
    }
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFaceWrapper()));
    interestQueue_->setDescription(NdnRtcUtils::toString("%s-iqueue", getDescription().c_str()));
    
    if (params.useVideo)
        videoConsumer_.reset(new VideoConsumer(params_, interestQueue_,
                                               rttEstimation_, videoRenderer));
    
    if (params_.useAudio)
        audioConsumer_.reset(new AudioConsumer(audioParams_, interestQueue_, rttEstimation_));
    
    serviceChannel_.reset(new ServiceChannel(this, interestQueue_));
    
    this->setLogger(new Logger(params_.loggingLevel,
                               NdnRtcUtils::toString("consumer-%s.log",
                                                     params.producerId)));
    isLoggerCreated_ = true;
    
    if (params.useAvSync && params.useAudio && params.useVideo)
    {
        shared_ptr<AudioVideoSynchronizer> avSync(new AudioVideoSynchronizer(videoConsumer_, audioConsumer_));
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
    serviceChannel_->startMonitor(*NdnRtcNamespace::getSessionInfoPrefix(params_));
    
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
    stat.videoStat_.srtt_ = rttEstimation_->getCurrentEstimation();
    
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
    interestQueue_->setLogger(logger);
    serviceChannel_->setLogger(logger);
    
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
ConsumerChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>&
                              prefix)
{
    // empty
}

void
ConsumerChannel::onProducerParametersUpdated(const ParamsStruct& newVideoParams,
                            const ParamsStruct& newAudioParams)
{
    LogInfoC << "producer parameters updated" << endl;
    
}

void
ConsumerChannel::onUpdateFailedWithTimeout()
{
    LogWarnC << "service update failed with timeout" << endl;
    serviceChannel_->startMonitor(*NdnRtcNamespace::getSessionInfoPrefix(params_));
}

void
ConsumerChannel::onUpdateFailedWithError(const char* errMsg)
{
    LogErrorC << "service update failed with message: " << string(errMsg) << endl;
}

