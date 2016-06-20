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
#include <boost/thread/lock_guard.hpp>

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
#include "params.h"

#define PARITY_RATIO 0.2

using namespace ndnrtc;
using namespace std;
using namespace ndn;
using namespace estimators;

typedef boost::shared_ptr<VideoFramePacket> FramePacketPtr;
typedef boost::future<FramePacketPtr> FutureFrame;
typedef boost::shared_ptr<boost::future<FramePacketPtr>> FutureFramePtr;

VideoStreamImpl::VideoStreamImpl(const std::string& streamPrefix,
	const MediaStreamSettings& settings, bool useFec):
MediaStreamBase(streamPrefix, settings),
playbackCounter_(0),
fecEnabled_(useFec)
{
	if (settings_.params_.type_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
		throw runtime_error("Wrong media stream parameters type supplied (audio instead of video)");
	
	description_ = "vstream-"+settings_.params_.streamName_;

	for (int i = 0; i < settings_.params_.getThreadNum(); ++i)
		if (settings_.params_.getVideoThread(i))
			add(settings_.params_.getVideoThread(i));

	PublisherSettings ps;
	ps.keyChain_  = settings_.keyChain_;
	ps.memoryCache_ = cache_.get();
	ps.segmentWireLength_ = settings_.params_.producerParams_.segmentSize_;
	ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshnessMs_;

	publisher_ = boost::make_shared<VideoPacketPublisher>(ps, streamPrefix_);
	publisher_->setDescription("seg-publisher-"+settings_.params_.streamName_);
}

VideoStreamImpl::~VideoStreamImpl()
{
	// cleanup
}

vector<string> VideoStreamImpl::getThreads() const
{
	boost::lock_guard<boost::mutex> scopedLock(internalMutex_);
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
	boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

	for (auto t:threads_) t.second->setLogger(logger);
	publisher_->setLogger(logger);
	dataPublisher_->setLogger(logger);
	ILoggingObject::setLogger(logger);
}

//******************************************************************************
void
VideoStreamImpl::add(const MediaThreadParams* mp)
{
	const VideoThreadParams* params = static_cast<const VideoThreadParams*>(mp);
	// check if thread already exists
	if (threads_.find(params->threadName_) != threads_.end())
	{
		stringstream ss;
		ss << "Thread " << params->threadName_ << " has been added already";
		throw runtime_error(ss.str());
	}
	else
	{
		boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

		threads_[params->threadName_] = boost::make_shared<VideoThread>(params->coderParams_);
		scalers_[params->threadName_] = boost::make_shared<FrameScaler>(params->coderParams_.encodeWidth_, 
			params->coderParams_.encodeHeight_);
		seqCounters_[params->threadName_].first  = 0;
		seqCounters_[params->threadName_].second = 0;
		metaKeepers_[params->threadName_] = boost::make_shared<MetaKeeper>(params);

		threads_[params->threadName_]->setDescription("thread-"+params->threadName_);
	}

	LogTraceC << "added thread " << params->threadName_ << std::endl;
}

void VideoStreamImpl::remove(const string& threadName)
{
	if (threads_.find(threadName) != threads_.end())
	{
		boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

		threads_.erase(threadName);
		scalers_.erase(threadName);
		seqCounters_.erase(threadName);
		metaKeepers_.erase(threadName);

		LogTraceC << "remove thread " << threadName << std::endl;
	}
}

void VideoStreamImpl::feedFrame(const WebRtcVideoFrame& frame)
{
	if (threads_.size())
	{
		boost::lock_guard<boost::mutex> scopedLock(internalMutex_);
		LogTraceC << "feeding frame "<< playbackCounter_ << " into threads..." << std::endl;

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

				LogTraceC << "encoded frame for thread " << it.first 
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
		packetHdr.sampleRate_ = metaKeepers_[it.first]->getRate();
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
		.appendSequenceNumber((isKey ? seqCounters_[thread].first : seqCounters_[thread].second));

	size_t nDataSeg = VideoFrameSegment::numSlices(*fp, 
			settings_.params_.producerParams_.segmentSize_);
	size_t nParitySeg = VideoFrameSegment::numSlices(*parityData, 
			settings_.params_.producerParams_.segmentSize_);
	PacketNumber pairedSeq = (isKey ? seqCounters_[thread].second : seqCounters_[thread].first);
	boost::shared_ptr<VideoStreamImpl> me = boost::static_pointer_cast<VideoStreamImpl>(shared_from_this());
	boost::shared_ptr<MetaKeeper> keeper = metaKeepers_[thread];

	async::dispatchAsync(settings_.faceIo_,  [me, nParitySeg, nDataSeg, pairedSeq, keeper, isKey, 
		thread, fp, parityData, dataName]{
		VideoFrameSegmentHeader segmentHdr;
		segmentHdr.totalSegmentsNum_ = nDataSeg;
		segmentHdr.paritySegmentsNum_ = nParitySeg;
		segmentHdr.playbackNo_ = me->playbackCounter_;
		segmentHdr.pairedSequenceNo_ = pairedSeq;

		size_t nSlices = me->publisher_->publish(dataName, *fp, segmentHdr);
		assert(nSlices);
		keeper->updateMeta(isKey, nDataSeg, nParitySeg);
	});

	dataName.append(NameComponents::NameComponentParity);
	async::dispatchAsync(settings_.faceIo_, [me, parityData, dataName]{
		size_t nParitySlices = me->dataPublisher_->publish(dataName, *parityData);
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

bool VideoStreamImpl::checkMeta()
{
	boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

	for (auto it:metaKeepers_)
	{
		if (it.second->isNewMetaAvailable())
		{
			Name metaName(streamPrefix_);
			metaName.append(it.first).append(NameComponents::NameComponentMeta)
				.appendVersion(it.second->getVersion());
			it.second->setVersion(it.second->getVersion()+1);
			dataPublisher_->publish(metaName, it.second->getMeta());
		}
	}

	return true;
}

//******************************************************************************
VideoStreamImpl::MetaKeeper::MetaKeeper(const VideoThreadParams* params):
BaseMetaKeeper(params),
rateMeter_(FreqMeter(boost::make_shared<TimeWindow>(500))),
deltaData_(Average(boost::make_shared<TimeWindow>(100))),
deltaParity_(Average(boost::make_shared<TimeWindow>(100))),
keyData_(Average(boost::make_shared<SampleWindow>(2))),
keyParity_(Average(boost::make_shared<SampleWindow>(2)))
{}

VideoStreamImpl::MetaKeeper::~MetaKeeper()
{}


bool
VideoStreamImpl::MetaKeeper::updateMeta(bool isKey, size_t nDataSeg, size_t nParitySeg)
{
	Average& dataAvg = (isKey ? keyData_ : deltaData_);
	Average& parityAvg = (isKey ? keyParity_ : deltaParity_);

	double r = rateMeter_.value();
	double d = deltaData_.value();
	double dp = deltaParity_.value();
	double k = keyData_.value();
	double kp = keyParity_.value();

	rateMeter_.newValue(0);
	dataAvg.newValue(nDataSeg);
	parityAvg.newValue(nParitySeg);

	newMeta_ = (r != rateMeter_.value() ||
		d != deltaData_.value() ||
		dp != deltaParity_.value() ||
		k != keyData_.value() ||
		kp != keyParity_.value());

	return newMeta_;
}

VideoThreadMeta
VideoStreamImpl::MetaKeeper::getMeta() const
{
	FrameSegmentsInfo segInfo;
	segInfo.deltaAvgSegNum_ = deltaData_.value();
	segInfo.deltaAvgParitySegNum_ = deltaParity_.value();
	segInfo.keyAvgSegNum_ = keyData_.value();
	segInfo.keyAvgParitySegNum_ = keyParity_.value();

	return boost::move(VideoThreadMeta(rateMeter_.value(),
			segInfo, ((VideoThreadParams*)params_)->coderParams_));
}

double
VideoStreamImpl::MetaKeeper::getRate() const
{ 
	return rateMeter_.value(); 
}
