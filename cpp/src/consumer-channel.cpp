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

using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
ConsumerChannel::ConsumerChannel(const ParamsStruct& params,
                                 const ParamsStruct& audioParams,
                                 const shared_ptr<FaceProcessor>& faceProcessor):
ndnrtc::NdnRtcObject(params),
isOwnFace_(false),
audioParams_(audioParams),
rttEstimation_(new RttEstimation()),
faceProcessor_(faceProcessor)
{
    if (!faceProcessor_.get())
    {
        isOwnFace_ = true;
        faceProcessor_ = FaceProcessor::createFaceProcessor(params_);
        faceProcessor_->getFace()->registerPrefix(*NdnRtcNamespace::getStreamKeyPrefix(params_),
                                                  bind(&ConsumerChannel::onInterest,
                                                       this, _1, _2, _3),
                                                  bind(&ConsumerChannel::onRegisterFailed,
                                                       this, _1));
    }
    
    interestQueue_.reset(new InterestQueue(faceProcessor_->getFace()));
    videoConsumer_.reset(new VideoConsumer(params_, interestQueue_));
}

//******************************************************************************
#pragma mark - public
int
ConsumerChannel::init()
{
#warning handle errors
    videoConsumer_->init();
    
    return RESULT_OK;
}

int
ConsumerChannel::startTransmission()
{
#warning handle errors
    if (isOwnFace_)
        faceProcessor_->startProcessing();
    
    videoConsumer_->start();
    
    return RESULT_OK;
}

int
ConsumerChannel::stopTransmission()
{
#warning handle errors
    videoConsumer_->stop();
    
    if (isOwnFace_)
        faceProcessor_->stopProcessing();
    
    return RESULT_OK;
}

void
ConsumerChannel::getChannelStatistics(ReceiverChannelStatistics &stat)
{
    stat.videoStat_.nBytesPerSec_ = 0;
    stat.videoStat_.interestFrequency_ = 0;
    stat.videoStat_.segmentsFrequency_ = 0;
    
    stat.videoStat_.srtt_ = rttEstimation_->getCurrentEstimation();
    
    stat.videoStat_.nPlayed_ = 0;
    stat.videoStat_.nMissed_ = 0;
    stat.videoStat_.nLost_ = 0;
    stat.videoStat_.nReceived_ = 0;
    
    videoConsumer_->getStatistics(stat.videoStat_);
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

