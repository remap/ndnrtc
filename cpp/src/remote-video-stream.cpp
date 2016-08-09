// 
// remote-video-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-video-stream.h"
#include <ndn-cpp/name.hpp>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "interfaces.h"
#include "video-playout.h"
#include "pipeline-control.h"
#include "pipeliner.h"
#include "latency-control.h"
#include "interest-control.h"
#include "playout-control.h"
#include "sample-validator.h"
#include "video-decoder.h"
#include "clock.h"

using namespace ndnrtc;
using namespace ndn;
using namespace boost;

RemoteVideoStreamImpl::RemoteVideoStreamImpl(boost::asio::io_service& io, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
RemoteStreamImpl(io, face, keyChain, streamPrefix)
{
    type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeVideo;
	playout_ = boost::make_shared<VideoPlayout>(io, playbackQueue_, sstorage_);
    playoutControl_ = boost::make_shared<PlayoutControl>(playout_, playbackQueue_, 150);
    validator_ = boost::make_shared<ManifestValidator>(face, keyChain);
    buffer_->attach(validator_.get());
}

RemoteVideoStreamImpl::~RemoteVideoStreamImpl()
{
    buffer_->detach(validator_.get());
}

void 
RemoteVideoStreamImpl::start(const std::string& threadName, 
            IExternalRenderer* renderer)
{
    assert(renderer);
    renderer_ = renderer;
    RemoteStreamImpl::start(threadName);
}

void
RemoteVideoStreamImpl::initiateFetching()
{   
    RemoteStreamImpl::initiateFetching();

    setupDecoder();
    setupPipelineControl();
}

void
RemoteVideoStreamImpl::setLogger(ndnlog::new_api::Logger* logger)
{
    RemoteStreamImpl::setLogger(logger);
    validator_->setLogger(logger);
}

#pragma mark private
void
RemoteVideoStreamImpl::feedFrame(const WebRtcVideoFrame& frame)
{
    uint8_t *rgbFrameBuffer = renderer_->getFrameBuffer(frame.width(),
        frame.height());
    
    if (rgbFrameBuffer)
    {
        ConvertFromI420(frame, webrtc::kBGRA, 0, rgbFrameBuffer);
        renderer_->renderBGRAFrame(clock::millisecondTimestamp(),
                                          frame.width(), frame.height(),
                                          rgbFrameBuffer);
    }
}

void
RemoteVideoStreamImpl::setupDecoder()
{
    boost::shared_ptr<RemoteVideoStreamImpl> me = boost::dynamic_pointer_cast<RemoteVideoStreamImpl>(shared_from_this());
    VideoThreadMeta meta(threadsMeta_[threadName_]->data());
    boost::shared_ptr<VideoDecoder> decoder = boost::make_shared<VideoDecoder>(meta.getCoderParams(),
     [this, me](const WebRtcVideoFrame& frame){
       feedFrame(frame);
     });
    boost::dynamic_pointer_cast<VideoPlayout>(playout_)->registerFrameConsumer(decoder.get());
    decoder_ = decoder;
}

void
RemoteVideoStreamImpl::setupPipelineControl()
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
       sstorage_));
    pipelineControl_->setLogger(logger_);
    segmentController_->attach(pipelineControl_.get());
    latencyControl_->registerObserver(pipelineControl_.get());
    pipelineControl_->start();
}