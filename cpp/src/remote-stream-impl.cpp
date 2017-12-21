// 
// remote-stream-impl.cpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-stream-impl.hpp"

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/name.hpp>

#include "async.hpp"
#include "buffer-control.hpp"
#include "clock.hpp"
#include "data-validator.hpp"
#include "drd-estimator.hpp"
#include "frame-buffer.hpp"
#include "interest-control.hpp"
#include "interest-queue.hpp"
#include "latency-control.hpp"
#include "pipeline-control-state-machine.hpp"
#include "pipeline-control.hpp"
#include "pipeliner.hpp"
#include "playout-control.hpp"
#include "playout.hpp"
#include "sample-estimator.hpp"
#include "rtx-controller.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace boost;

RemoteStreamImpl::RemoteStreamImpl(asio::io_service& io, 
			const shared_ptr<ndn::Face>& face,
			const shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
type_(MediaStreamParams::MediaStreamType::MediaStreamTypeUnknown),
io_(io),
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
	rtxController_ = make_shared<RetransmissionController>(sstorage_, playbackQueue_);
	buffer_->attach(rtxController_.get());
	// playout and playout-control created in subclasses

	interestQueue_ = make_shared<InterestQueue>(io, face, sstorage_);
	shared_ptr<DrdEstimator> drdEstimator(make_shared<DrdEstimator>());
	sampleEstimator_ = make_shared<SampleEstimator>(sstorage_);
	bufferControl_ = make_shared<BufferControl>(drdEstimator, buffer_, sstorage_);
	latencyControl_ = make_shared<LatencyControl>(1000, drdEstimator, sstorage_);
	interestControl_ = make_shared<InterestControl>(drdEstimator, sstorage_);
	
	// pipeliner and pipeline control created in subclasses

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
	if (pipeliner_.get())
	{
		pipeliner_->setInterestLifetime(lifetimeMs);
	}
	else
		LogWarnC << "attempting to setInterestLifetime() but pipeliner_ is null" << std::endl;
}

void 
RemoteStreamImpl::setTargetBufferSize(unsigned int bufferSizeMs)
{
	if (playoutControl_.get())
	{
    	playoutControl_->setThreshold(bufferSizeMs);
		LogDebugC << "set target buffer size to " << bufferSizeMs << "ms" << std::endl;
    	(*sstorage_)[Indicator::BufferTargetSize] = bufferSizeMs;
    }
    else
    	#warning figure this out for audio streams
    	LogWarnC << "attempting to setTargetBufferSize() but playoutControl_ is null" << std::endl;
}

void 
RemoteStreamImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
	NdnRtcComponent::setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(buffer_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(metaFetcher_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(bufferControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(interestQueue_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(pipeliner_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(latencyControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(interestControl_)->setLogger(logger);
    dynamic_pointer_cast<NdnRtcComponent>(playbackQueue_)->setLogger(logger);
    segmentController_->setLogger(logger);
    rtxController_->setLogger(logger);
    if (pipelineControl_.get()) pipelineControl_->setLogger(logger);
}

bool
RemoteStreamImpl::isVerified() const
{
	return (validationInfo_.size() == 0);
}

void
RemoteStreamImpl::attach(IRemoteStreamObserver* o)
{
    async::dispatchSync(io_, [this, o](){
        observers_.push_back(o);
    });
}

void
RemoteStreamImpl::detach(IRemoteStreamObserver* o)
{
    async::dispatchSync(io_, [this, o](){
        std::vector<IRemoteStreamObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), o);
        if (it != observers_.end())
            observers_.erase(it);
    });
}

statistics::StatisticsStorage
RemoteStreamImpl::getStatistics() const
{
    (*sstorage_)[Indicator::Timestamp] = clock::millisecSinceEpoch();
    return *sstorage_;
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

		// notify observers that we got all the meta needed
		notifyObservers(RemoteStream::Event::NewMeta);

		if (cuedToRun_ && !isRunning_)
			initiateFetching();
	}
}

void
RemoteStreamImpl::initiateFetching()
{
	if (threadName_ == "")
	{
		threadName_ = threadsMeta_.begin()->first;
	}
	else
	{
		if (threadsMeta_.find(threadName_) == threadsMeta_.end())
		{
			LogErrorC << "Can't find requested thread " << threadName_
			<< " in received metadata" << std::endl;
			throw std::runtime_error("Can't find requested thread to fetch");
		}
	}

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
    }
}

void
RemoteStreamImpl::addValidationInfo(const std::vector<ValidationErrorInfo>& validationInfo)
{
	for (auto& vi:validationInfo)
		LogWarnC << "failed to verify data packet " << vi.getData()->getName() << std::endl;
	std::copy(validationInfo.begin(), validationInfo.end(), std::back_inserter(validationInfo_));
}

void
RemoteStreamImpl::notifyObservers(RemoteStream::Event ev)
{
	for (auto o : observers_)
		o->onNewEvent(RemoteStream::Event::NewMeta);
}
