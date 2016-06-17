// 
// remote-stream-impl.cpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-stream-impl.h"

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/name.hpp>

#include "frame-buffer.h"
#include "buffer-control.h"
#include "drd-estimator.h"
#include "interest-control.h"
#include "latency-control.h"
#include "pipeline-control.h"
#include "pipeliner.h"
#include "sample-estimator.h"
#include "pipeline-control-state-machine.h"
#include "playout.h"
#include "playout-control.h"
#include "interest-queue.h"

using namespace ndnrtc;
using namespace ndn;

RemoteStreamImpl::RemoteStreamImpl(boost::asio::io_service& io, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
io_(io)
{
	boost::shared_ptr<SegmentController> segmentController(boost::make_shared<SegmentController>(io, 500));

	boost::shared_ptr<statistics::StatisticsStorage> statStorage(statistics::StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(boost::make_shared<SlotPool>(500)));
	boost::shared_ptr<PlaybackQueue> playbackQueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	// playout and playout-control created in subclasses

	boost::shared_ptr<InterestQueue> interestQueue(boost::make_shared<InterestQueue>(io, face, statStorage));
	boost::shared_ptr<DrdEstimator> drdEstimator(boost::make_shared<DrdEstimator>());
	boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>());
	boost::shared_ptr<LatencyControl> latencyControl(boost::make_shared<LatencyControl>(1000, drdEstimator));
	boost::shared_ptr<InterestControl> interestControl(boost::make_shared<InterestControl>(drdEstimator));
	boost::shared_ptr<BufferControl> bufferControl(boost::make_shared<BufferControl>(drdEstimator, buffer));
	
	PipelinerSettings pps;
	pps.interestLifetimeMs_ = 2000;
	pps.sampleEstimator_ = sampleEstimator;
	pps.buffer_ = buffer;
	pps.interestControl_ = interestControl;
	pps.interestQueue_ = interestQueue;
	pps.playbackQueue_ = playbackQueue;
	pps.segmentController_ = segmentController;

	boost::shared_ptr<Pipeliner> pipeliner(boost::make_shared<Pipeliner>(pps));
	// pipeline control created in subclasses

	segmentController->attach(sampleEstimator.get());
	segmentController->attach(bufferControl.get());

	drdEstimator->attach(interestControl.get());
	drdEstimator->attach(latencyControl.get());

	bufferControl->attach(interestControl.get());
	bufferControl->attach(latencyControl.get());

	// buffer->attach(pipeliner);
	buffer->attach(playbackQueue.get());
}

bool
RemoteStreamImpl::isMetaFetched() const 
{
	return false;
}

std::vector<std::string> 
RemoteStreamImpl::getThreads() const
{
	return std::vector<std::string>();
}

void RemoteStreamImpl::start(const std::string& threadName)
{
	pipelineControl_->start();
}

void 
RemoteStreamImpl::setThread(const std::string& threadName)
{
	
}

void RemoteStreamImpl::stop()
{
	pipelineControl_->stop();
}

void 
RemoteStreamImpl::setInterestLifetime(unsigned int lifetimeMs)
{
	// pipeliner_->setInterestLifetime(lifetimeMs);
}

void 
RemoteStreamImpl::setTargetBufferSize(unsigned int bufferSizeMs)
{
	// playoutControl_->setThreshold(bufferSizeMs);
}

void 
RemoteStreamImpl::setLogger(ndnlog::new_api::Logger* logger)
{

}

void
RemoteStreamImpl::attach(IRemoteStreamObserver* o)
{
	observers_.push_back(o);
}

void
RemoteStreamImpl::detach(IRemoteStreamObserver* o)
{
	observers_.erase(std::find(observers_.begin(), observers_.end(), o));
}

