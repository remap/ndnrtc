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
#include "data-validator.h"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace boost;

RemoteStreamImpl::RemoteStreamImpl(asio::io_service& io, 
			const shared_ptr<ndn::Face>& face,
			const shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
type_(MediaStreamParams::MediaStreamType::MediaStreamTypeUnknown),
face_(face),
keyChain_(keyChain),
streamPrefix_(streamPrefix),
needMeta_(true), isRunning_(false), cuedToRun_(false),
metaFetcher_(make_shared<MetaFetcher>()),
sstorage_(StatisticsStorage::createConsumerStatistics())
{
	assert(face.get());
	assert(keyChain.get());

	description_ = "remote-stream";

	segmentController_ = make_shared<SegmentController>(io, 500, sstorage_);
	buffer_ = make_shared<Buffer>(sstorage_, make_shared<SlotPool>(500));
	playbackQueue_ = make_shared<PlaybackQueue>(Name(streamPrefix),
                                               dynamic_pointer_cast<Buffer>(buffer_));
	// playout and playout-control created in subclasses

	interestQueue_ = make_shared<InterestQueue>(io, face, sstorage_);
	shared_ptr<DrdEstimator> drdEstimator(make_shared<DrdEstimator>());
	sampleEstimator_ = make_shared<SampleEstimator>(sstorage_);
	bufferControl_ = make_shared<BufferControl>(drdEstimator, buffer_, sstorage_);
	latencyControl_ = make_shared<LatencyControl>(1000, drdEstimator);
	interestControl_ = make_shared<InterestControl>(drdEstimator);
	
	PipelinerSettings pps;
	pps.interestLifetimeMs_ = 2000;
	pps.sampleEstimator_ = sampleEstimator_;
	pps.buffer_ = buffer_;
	pps.interestControl_ = interestControl_;
	pps.interestQueue_ = interestQueue_;
	pps.playbackQueue_ = playbackQueue_;
	pps.segmentController_ = segmentController_;
    pps.sstorage_ = sstorage_;

	pipeliner_ = make_shared<Pipeliner>(pps);
	// pipeline control created in subclasses

	segmentController_->attach(sampleEstimator_.get());
	segmentController_->attach(bufferControl_.get());

	drdEstimator->attach((InterestControl*)interestControl_.get());
	drdEstimator->attach((LatencyControl*)latencyControl_.get());

	bufferControl_->attach((InterestControl*)interestControl_.get());
	bufferControl_->attach((LatencyControl*)latencyControl_.get());
}

bool
RemoteStreamImpl::isMetaFetched() const 
{
	return streamMeta_.get() && 
		streamMeta_->getThreads().size() == threadsMeta_.size();
}

std::vector<std::string> 
RemoteStreamImpl::getThreads() const
{
	if (streamMeta_.get())
		return streamMeta_->getThreads();
	return std::vector<std::string>();
}

void RemoteStreamImpl::start(const std::string& threadName)
{
	if (isRunning_)
		throw std::runtime_error("Remote stream has been already started");

    cuedToRun_ = true;
    threadName_ = threadName;
    
	if (!needMeta_)
		initiateFetching();
}

void 
RemoteStreamImpl::setThread(const std::string& threadName)
{
	threadName_ = threadName;
}

void RemoteStreamImpl::stop()
{
    cuedToRun_ = false;
	stopFetching();
}

void 
RemoteStreamImpl::setInterestLifetime(unsigned int lifetimeMs)
{
	// pipeliner_->setInterestLifetime(lifetimeMs);
}

void 
RemoteStreamImpl::setTargetBufferSize(unsigned int bufferSizeMs)
{
    playoutControl_->setThreshold(bufferSizeMs);
	LogDebugC << "set target buffer size to " << bufferSizeMs << "ms" << std::endl;
}

void 
RemoteStreamImpl::setLogger(ndnlog::new_api::Logger* logger)
{
	NdnRtcComponent::setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(buffer_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(metaFetcher_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(bufferControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(playout_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(playoutControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(interestQueue_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(pipeliner_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(latencyControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(interestControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(playout_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(playbackQueue_)->setLogger(logger);
    if (pipelineControl_.get()) pipelineControl_->setLogger(logger);
}

bool
RemoteStreamImpl::isVerified() const
{
	return (validationInfo_.size() == 0);
}

#pragma mark - private
void
RemoteStreamImpl::fetchMeta()
{
	shared_ptr<RemoteStreamImpl> me = dynamic_pointer_cast<RemoteStreamImpl>(shared_from_this());
	metaFetcher_->fetch(face_, keyChain_, Name(streamPrefix_).append(NameComponents::NameComponentMeta), 
		[me,this](NetworkData& meta, 
			const std::vector<ValidationErrorInfo>& validationInfo){ 
			me->addValidationInfo(validationInfo);
			me->streamMetaFetched(meta);
		},
		[me,this](const std::string& msg){ 
			LogWarnC << "error fetching stream meta: " << msg << std::endl;
			if (needMeta_ && !metaFetcher_->hasPendingRequest()) me->fetchMeta(); 
		});
}

void
RemoteStreamImpl::streamMetaFetched(NetworkData& meta)
{
	streamMeta_ = make_shared<MediaStreamMeta>(move(meta));

	shared_ptr<RemoteStreamImpl> me = dynamic_pointer_cast<RemoteStreamImpl>(shared_from_this());
	std::stringstream ss;
	for (auto& t:streamMeta_->getThreads()) 
	{
		fetchThreadMeta(t);
		ss << t << " ";
	}
	LogInfoC << "received stream meta info. " 
		<< streamMeta_->getThreads().size() << " thread(s): " << ss.str() << std::endl;
}

void 
RemoteStreamImpl::fetchThreadMeta(const std::string& threadName)
{
	shared_ptr<RemoteStreamImpl> me = dynamic_pointer_cast<RemoteStreamImpl>(shared_from_this());
	metaFetcher_->fetch(face_, keyChain_, Name(streamPrefix_).append(threadName).append(NameComponents::NameComponentMeta),
		[threadName,me,this](NetworkData& meta, 
			const std::vector<ValidationErrorInfo>& validationInfo){ 
			me->addValidationInfo(validationInfo);
			me->threadMetaFetched(threadName, meta);
		},
		[me,threadName,this](const std::string& msg){ 
			LogWarnC << "error fetching thread meta: " << msg << std::endl;
			if (needMeta_ && !metaFetcher_->hasPendingRequest()) me->fetchThreadMeta(threadName);
	});
}

void 
RemoteStreamImpl::threadMetaFetched(const std::string& thread, NetworkData& meta)
{
	threadsMeta_[thread] = make_shared<NetworkData>(move(meta));
	LogInfoC << "received thread meta info for: " << thread << std::endl;

	if (threadsMeta_.size() == streamMeta_->getThreads().size())
	{
		needMeta_ = false;
		if (cuedToRun_ && !isRunning_)
			initiateFetching();
	}
}

void
RemoteStreamImpl::initiateFetching()
{
    LogInfoC << "initiating fetching from " << streamPrefix_
        << " (thread " << threadName_ << ")" << std::endl;
    
    isRunning_ = true;
    segmentController_->setIsActive(true);
    
    if (type_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
    {
        VideoThreadMeta meta(threadsMeta_[threadName_]->data());
        sampleEstimator_->bootstrapSegmentNumber(meta.getSegInfo().deltaAvgSegNum_,
            SampleClass::Delta, SegmentClass::Data);
        sampleEstimator_->bootstrapSegmentNumber(meta.getSegInfo().deltaAvgParitySegNum_,
            SampleClass::Delta, SegmentClass::Parity);
        sampleEstimator_->bootstrapSegmentNumber(meta.getSegInfo().keyAvgSegNum_,
            SampleClass::Key, SegmentClass::Data);
        sampleEstimator_->bootstrapSegmentNumber(meta.getSegInfo().keyAvgParitySegNum_,
            SampleClass::Key, SegmentClass::Parity);
    }
}

void 
RemoteStreamImpl::stopFetching()
{
    if (isRunning_)
    {
        segmentController_->setIsActive(false);
        pipelineControl_->stop();
        interestQueue_->reset();
        isRunning_ = false;
        needMeta_ = false;
        streamMeta_.reset();
        threadsMeta_.clear();
    }
}

void
RemoteStreamImpl::addValidationInfo(const std::vector<ValidationErrorInfo>& validationInfo)
{
	for (auto& vi:validationInfo)
		LogWarnC << "failed to verify data packet " << vi.getData()->getName() << std::endl;
	std::copy(validationInfo.begin(), validationInfo.end(), std::back_inserter(validationInfo_));
}
