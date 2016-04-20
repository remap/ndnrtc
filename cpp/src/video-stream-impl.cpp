// 
// video-stream-impl.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread/future.hpp>
#include <boost/asio.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <ndn-cpp/c/common.h>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "video-stream-impl.h"
#include "frame-data.h"
#include "video-thread.h"
#include "video-coder.h"
#include "packet-publisher.h"
#include "name-components.h"
#include "simple-log.h"
#include "estimators.h"
#include "clock.h"
#include "async.h"

#define PARITY_RATIO 0.2

using namespace ndnrtc;
using namespace std;
using namespace ndn;

typedef boost::shared_ptr<VideoFramePacket> FramePacketPtr;
typedef boost::future<FramePacketPtr> FutureFrame;
typedef boost::shared_ptr<boost::future<FramePacketPtr>> FutureFramePtr;

VideoStreamImpl::VideoStreamImpl(const std::string& streamPrefix,
	const MediaStreamSettings& settings, bool useFec):
fecEnabled_(useFec),
streamPrefix_(NameComponents::videoStreamPrefix(streamPrefix)),
settings_(settings),
cache_(boost::make_shared<ndn::MemoryContentCache>(settings.face_))
{
	if (settings_.params_.type_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
		throw runtime_error("Wrong media stream parameters type supplied (audio instead of video)");
	
	description_ = "vstream-"+settings_.params_.streamName_;
	streamPrefix_.append(Name(settings_.params_.streamName_));

	for (int i = 0; i < settings_.params_.getThreadNum(); ++i)
		if (settings_.params_.getVideoThread(i))
			addThread(settings_.params_.getVideoThread(i));

	PublisherSettings ps;
	ps.keyChain_  = settings_.keyChain_;
	ps.memoryCache_ = cache_.get();
	ps.segmentWireLength_ = settings_.params_.producerParams_.segmentSize_;
	ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshnessMs_;

	publisher_ = boost::make_shared<VideoPacketPublisher>(ps, streamPrefix_);
	publisher_->setDescription("publisher-"+settings_.params_.streamName_);
	parityPublisher_ = boost::make_shared<CommonPacketPublisher>(ps);
	parityPublisher_->setDescription("par-publisher-"+settings_.params_.streamName_);
}

VideoStreamImpl::~VideoStreamImpl()
{
	// cleanup
}

void
VideoStreamImpl::addThread(const VideoThreadParams* params)
{
	// check if thread already exists
	if (threads_.find(params->threadName_) != threads_.end())
	{
		stringstream ss;
		ss << "Thread " << params->threadName_ << " has been added already";
		throw runtime_error(ss.str());
	}

	threads_[params->threadName_] = boost::make_shared<VideoThread>(params->coderParams_);
	scalers_[params->threadName_] = boost::make_shared<FrameScaler>(params->coderParams_.encodeWidth_, 
		params->coderParams_.encodeHeight_);
	seqCounters_[params->threadName_].first  = 0;
	seqCounters_[params->threadName_].second = 0;

	rateEstimators_[params->threadName_] = estimators::setupFrequencyMeter(2);

	LogTraceC << "added thread " << params->threadName_ << std::endl;
}

void VideoStreamImpl::removeThread(const string& threadName)
{
	if (threads_.find(threadName) != threads_.end())
	{
		threads_.erase(threadName);
		scalers_.erase(threadName);
		seqCounters_.erase(threadName);
		
		estimators::releaseFrequencyMeter(rateEstimators_[threadName]);
		rateEstimators_.erase(threadName);

		LogTraceC << "remove thread " << threadName << std::endl;
	}
}

string VideoStreamImpl::getPrefix()
{
	return streamPrefix_.toUri();
}

vector<string> VideoStreamImpl::getThreads()
{
	std::vector<string> threads;

	for (auto it:threads_)
		threads.push_back(it.first);

	return threads;
}

void VideoStreamImpl::incomingFrame(const ArgbRawFrameWrapper& w)
{
	LogDebugC << "incoming ARGB frame " << w.width_ << "x" << w.height_ << std::endl;
	feedFrame(conv_ << w);
}

void VideoStreamImpl::incomingFrame(const I420RawFrameWrapper& w)
{
	LogDebugC << "incoming I420 frame " << w.width_ << "x" << w.height_ << std::endl;
	feedFrame(conv_ << w);
}

void VideoStreamImpl::setLogger(ndnlog::new_api::Logger* logger)
{
	for (auto t:threads_) t.second->setLogger(logger);
	publisher_->setLogger(logger);
	parityPublisher_->setLogger(logger);
	ILoggingObject::setLogger(logger);
}

//******************************************************************************
void VideoStreamImpl::feedFrame(const WebRtcVideoFrame& frame)
{
	if (threads_.size())
	{
		LogTraceC << "feeding frame into threads..." << std::endl;

		map<string,FutureFramePtr> futureFrames;
		for (auto it:threads_)
		{
			FutureFramePtr ff = 
			boost::make_shared<FutureFrame>(boost::move(boost::async(boost::launch::async, 
				boost::bind(&VideoThread::encode, it.second.get(), (*scalers_[it.first])(frame)))));
			futureFrames[it.first] = ff;
		}

		map<string, FramePacketPtr> frames;
		for (auto it:futureFrames)
		{
			FramePacketPtr f(it.second->get());

			if (f.get())
			{
				frames[it.first] = f;

				LogTraceC << "encoded frame from thread " << it.first 
				<< " " << f->getLength() << " bytes" << std::endl;
			}
		}
		publish(frames);
		playbackCounter_++;
	}
	else
		LogWarnC << "incoming frame was given, but there are no threads" << std::endl;
}

void VideoStreamImpl::publish(map<string, FramePacketPtr>& frames)
{
	LogTraceC << "publishing " << frames.size() << " frames" << std::endl;

	for (auto it:frames)
	{
		// prepare packet header
		bool isKey = (it.second->getFrame()._frameType == webrtc::kKeyFrame);
		CommonHeader packetHdr;
		packetHdr.sampleRate_ = estimators::currentFrequencyMeterValue(rateEstimators_[it.first]);
		packetHdr.publishTimestampMs_ = clock::millisecondTimestamp();
		packetHdr.publishUnixTimestampMs_ = clock::unixTimestamp();

		it.second->setSyncList(getCurrentSyncList(isKey));
		it.second->setHeader(packetHdr);

		publish(it.first, it.second);

		if (isKey) seqCounters_[it.first].first++;
		else seqCounters_[it.first].second++;
	}
}

void VideoStreamImpl::publish(const string& thread, FramePacketPtr& fp)
{
	boost::shared_ptr<NetworkData> parityData = fp->getParityData(
		VideoFrameSegment::payloadLength(settings_.params_.producerParams_.segmentSize_),
		PARITY_RATIO);

	bool isKey = (fp->getFrame()._frameType == webrtc::kKeyFrame);
	Name dataName(streamPrefix_);
	dataName.append(thread)
		.append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
		.append(Name::Component::fromNumber(isKey ? seqCounters_[thread].first : seqCounters_[thread].second));

	boost::shared_ptr<VideoStreamImpl> me = boost::static_pointer_cast<VideoStreamImpl>(shared_from_this());
	async::dispatchAsync(settings_.faceIo_,  [me, thread, fp, parityData, dataName]{
		bool isKey = (fp->getFrame()._frameType == webrtc::kKeyFrame);
		VideoFrameSegmentHeader segmentHdr;
		segmentHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(*fp, 
			me->settings_.params_.producerParams_.segmentSize_);
		segmentHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, 
			me->settings_.params_.producerParams_.segmentSize_);
		segmentHdr.playbackNo_ = me->playbackCounter_;
		segmentHdr.pairedSequenceNo_ = (isKey ? 
			me->seqCounters_[thread].second : 
			me->seqCounters_[thread].first);
		size_t nSlices = me->publisher_->publish(dataName, *fp, segmentHdr);
		assert(nSlices);
	});

	dataName.append(NameComponents::NameComponentParity);
	async::dispatchAsync(settings_.faceIo_, [me, parityData, dataName]{
		DataSegmentHeader dummy;
		size_t nParitySlices = me->parityPublisher_->publish(dataName, *parityData, dummy);
		assert(nParitySlices);
	});
}

map<string, PacketNumber> 
VideoStreamImpl::getCurrentSyncList(bool forKey)
{
	map<string, PacketNumber> syncList;
	for (auto it:seqCounters_)
		syncList[it.first] = (forKey ? it.second.first : it.second.second);
	return syncList;
}
