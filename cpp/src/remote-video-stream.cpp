//
// remote-video-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-video-stream.hpp"
#include <ndn-cpp/name.hpp>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "interfaces.hpp"
#include "video-playout.hpp"
#include "pipeline-control.hpp"
#include "pipeliner.hpp"
#include "latency-control.hpp"
#include "interest-control.hpp"
#include "playout-control.hpp"
#include "sample-validator.hpp"
#include "video-decoder.hpp"
#include "clock.hpp"

using namespace ndnrtc;
using namespace ndn;
using namespace boost;

RemoteVideoStreamImpl::RemoteVideoStreamImpl(boost::asio::io_service &io,
                                             const boost::shared_ptr<ndn::Face> &face,
                                             const boost::shared_ptr<ndn::KeyChain> &keyChain,
                                             const std::string &streamPrefix) 
    : RemoteStreamImpl(io, face, keyChain, streamPrefix)
{
    type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeVideo;

    PipelinerSettings pps;
    pps.interestLifetimeMs_ = 2000;
    pps.sampleEstimator_ = sampleEstimator_;
    pps.buffer_ = buffer_;
    pps.interestControl_ = interestControl_;
    pps.interestQueue_ = interestQueue_;
    pps.playbackQueue_ = playbackQueue_;
    pps.segmentController_ = segmentController_;
    pps.sstorage_ = sstorage_;

    pipeliner_ = make_shared<Pipeliner>(pps, boost::make_shared<Pipeliner::VideoNameScheme>());
    playout_ = boost::make_shared<VideoPlayout>(io, playbackQueue_, sstorage_);
    playoutControl_ = boost::make_shared<PlayoutControl>(playout_, playbackQueue_, rtxController_, 150);
    playbackQueue_->attach(playoutControl_.get());
    latencyControl_->setPlayoutControl(playoutControl_);

    validator_ = boost::make_shared<ManifestValidator>(face, keyChain, sstorage_);
    buffer_->attach(validator_.get());
}

RemoteVideoStreamImpl::~RemoteVideoStreamImpl()
{
    buffer_->detach(validator_.get());
}

void RemoteVideoStreamImpl::start(const std::string &threadName,
                                  IExternalRenderer *renderer)
{
    assert(renderer);
    renderer_ = renderer;
    RemoteStreamImpl::start(threadName);
}

void RemoteVideoStreamImpl::initiateFetching()
{
    RemoteStreamImpl::initiateFetching();

    setupDecoder();
    setupPipelineControl();
    pipelineControl_->start();
}

void RemoteVideoStreamImpl::stopFetching()
{
    RemoteStreamImpl::stopFetching();

    releasePipelineControl();
    releaseDecoder();
}

void RemoteVideoStreamImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    RemoteStreamImpl::setLogger(logger);
    validator_->setLogger(logger);
    boost::dynamic_pointer_cast<NdnRtcComponent>(playoutControl_)->setLogger(logger);
    boost::dynamic_pointer_cast<Playout>(playout_)->setLogger(logger);
}

#pragma mark private
void RemoteVideoStreamImpl::feedFrame(PacketNumber frameNo, const WebRtcVideoFrame &frame)
{
    uint8_t *rgbFrameBuffer = renderer_->getFrameBuffer(frame.width(),
                                                        frame.height());

    if (rgbFrameBuffer)
    {
        LogTraceC << "passing frame " << frameNo << "p to renderer" << std::endl;
#warning this needs to be tested with frames captured from real video devices
        ConvertFromI420(frame, webrtc::kBGRA, 0, rgbFrameBuffer);
        renderer_->renderBGRAFrame(clock::millisecondTimestamp(), frameNo,
                                   frame.width(), frame.height(),
                                   rgbFrameBuffer);
    }
    else
        LogTraceC << "renderer is busy." << std::endl;
}

void RemoteVideoStreamImpl::setupDecoder()
{
    boost::shared_ptr<RemoteVideoStreamImpl> me = boost::dynamic_pointer_cast<RemoteVideoStreamImpl>(shared_from_this());
    VideoThreadMeta meta(threadsMeta_[threadName_]->data());
    boost::shared_ptr<VideoDecoder> decoder =
        boost::make_shared<VideoDecoder>(meta.getCoderParams(),
                                         [this, me](PacketNumber frameNo, const WebRtcVideoFrame &frame) 
                                         {
                                            feedFrame(frameNo, frame);
                                         });
    boost::dynamic_pointer_cast<VideoPlayout>(playout_)->registerFrameConsumer(decoder.get());
    decoder_ = decoder;
}

void RemoteVideoStreamImpl::releaseDecoder()
{
    dynamic_pointer_cast<VideoPlayout>(playout_)->deregisterFrameConsumer();
    decoder_.reset();
}

void RemoteVideoStreamImpl::setupPipelineControl()
{
    Name threadPrefix(streamPrefix_);
    threadPrefix.append(threadName_);

    pipelineControl_ = boost::make_shared<PipelineControl>(
        PipelineControl::videoPipelineControl(threadPrefix.toUri(),
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

void RemoteVideoStreamImpl::releasePipelineControl()
{
    latencyControl_->unregisterObserver();
    segmentController_->detach(pipelineControl_.get());

    pipelineControl_.reset();
}
