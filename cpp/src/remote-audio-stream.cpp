//
// remote-audio-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-audio-stream.hpp"
#include "audio-playout.hpp"
#include "frame-data.hpp"
#include "pipeline-control.hpp"
#include "pipeliner.hpp"
#include "latency-control.hpp"
#include "interest-control.hpp"
#include "playout-control.hpp"
#include "sample-validator.hpp"

using namespace ndn;
using namespace ndnrtc;

RemoteAudioStreamImpl::RemoteAudioStreamImpl(boost::asio::io_service &io,
                                             const boost::shared_ptr<ndn::Face> &face,
                                             const boost::shared_ptr<ndn::KeyChain> &keyChain,
                                             const std::string &streamPrefix)
    : RemoteStreamImpl(io, face, keyChain, streamPrefix)
    , io_(io)
{   
    construct();
    // live stream has data-driven interest-control (pipeline)
    bufferControl_->attach((InterestControl *)interestControl_.get());
}

void RemoteAudioStreamImpl::construct()
{
    type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;

    PipelinerSettings pps;
    pps.interestLifetimeMs_ = 2000;
    pps.sampleEstimator_ = sampleEstimator_;
    pps.buffer_ = buffer_;
    pps.interestControl_ = interestControl_;
    pps.interestQueue_ = interestQueue_;
    pps.playbackQueue_ = playbackQueue_;
    pps.segmentController_ = segmentController_;
    pps.sstorage_ = sstorage_;

    pipeliner_ = boost::make_shared<Pipeliner>(pps,
                                               boost::make_shared<Pipeliner::AudioNameScheme>());
    validator_ = boost::make_shared<SampleValidator>(keyChain_, sstorage_);
    buffer_->attach(validator_.get());
}

RemoteAudioStreamImpl::~RemoteAudioStreamImpl()
{
    buffer_->detach(validator_.get());
}

void RemoteAudioStreamImpl::initiateFetching()
{
    RemoteStreamImpl::initiateFetching();

    setupPlayout();
    setupPipelineControl();
    pipelineControl_->start(threadsMeta_[threadName_]);
}

void RemoteAudioStreamImpl::stopFetching()
{
    RemoteStreamImpl::stopFetching();

    releasePlayout();
    releasePipelineControl();
}

void RemoteAudioStreamImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    RemoteStreamImpl::setLogger(logger);
    validator_->setLogger(logger);
}

void RemoteAudioStreamImpl::setupPlayout()
{
    AudioThreadMeta meta(threadsMeta_[threadName_]->data());
    playout_ = boost::make_shared<AudioPlayout>(io_, playbackQueue_, sstorage_,
                                                WebrtcAudioChannel::fromString(meta.getCodec()));
    playoutControl_ = boost::make_shared<PlayoutControl>(playout_, playbackQueue_, 
                                                         rtxController_);
    playbackQueue_->attach(playoutControl_.get());
    latencyControl_->setPlayoutControl(playoutControl_);
    drdEstimator_->attach(playoutControl_.get());

    boost::dynamic_pointer_cast<Playout>(playout_)->setLogger(logger_);
    boost::dynamic_pointer_cast<NdnRtcComponent>(playoutControl_)->setLogger(logger_);
}

void RemoteAudioStreamImpl::releasePlayout()
{
    playbackQueue_->detach(playoutControl_.get());
    drdEstimator_->detach(playoutControl_.get());
    playout_.reset();
    playoutControl_.reset();
}

void RemoteAudioStreamImpl::setupPipelineControl()
{
    Name threadPrefix(getStreamPrefix());
    threadPrefix.append(threadName_);

    pipelineControl_ = boost::make_shared<PipelineControl>(
        PipelineControl::defaultPipelineControl(threadPrefix.toUri(),
                                                drdEstimator_,
                                                boost::dynamic_pointer_cast<IBuffer>(buffer_),
                                                boost::dynamic_pointer_cast<IPipeliner>(pipeliner_),
                                                boost::dynamic_pointer_cast<IInterestControl>(interestControl_),
                                                boost::dynamic_pointer_cast<ILatencyControl>(latencyControl_),
                                                boost::dynamic_pointer_cast<IPlayoutControl>(playoutControl_),
                                                sampleEstimator_,
                                                sstorage_));
    pipelineControl_->setLogger(logger_);
    rtxController_->attach(pipelineControl_.get());
    segmentController_->attach(pipelineControl_.get());
    latencyControl_->registerObserver(pipelineControl_.get());
}

void RemoteAudioStreamImpl::releasePipelineControl()
{
    latencyControl_->unregisterObserver();
    segmentController_->detach(pipelineControl_.get());
}
