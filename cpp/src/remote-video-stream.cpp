// 
// remote-video-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-video-stream.h"
#include <ndn-cpp/name.hpp>

#include "video-playout.h"
#include "pipeline-control.h"
#include "pipeliner.h"
#include "latency-control.h"
#include "interest-control.h"
#include "playout-control.h"

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
}

void
RemoteVideoStreamImpl::initiateFetching()
{   
    RemoteStreamImpl::initiateFetching();
    
	Name threadPrefix(streamPrefix_);
	threadPrefix.append(threadName_);

	// create pipeline control
	pipelineControl_ = boost::make_shared<PipelineControl>(
		PipelineControl::videoPipelineControl(threadPrefix.toUri(),
            boost::dynamic_pointer_cast<IPipeliner>(pipeliner_),
            boost::dynamic_pointer_cast<IInterestControl>(interestControl_),
            boost::dynamic_pointer_cast<ILatencyControl>(latencyControl_)));
    pipelineControl_->setLogger(logger_);
    segmentController_->attach(pipelineControl_.get());
    latencyControl_->registerObserver(pipelineControl_.get());
    pipelineControl_->start();
}
