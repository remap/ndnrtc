// 
// test-audio-playout.cc
//
//  Created by Peter Gusev on 18 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "frame-data.hpp"
#include "src/audio-playout.hpp"
#include "audio-thread.hpp"
#include "clock.hpp"
#include "include/params.hpp"
#include "audio-capturer.hpp"
#include "statistics.hpp"

#include "mock-objects/audio-thread-callback-mock.hpp"
#include "mock-objects/buffer-observer-mock.hpp"
#include "mock-objects/playback-queue-observer-mock.hpp"
#include "mock-objects/playout-observer-mock.hpp"

// #define ENABLE_LOGGING

using namespace testing;
using namespace ndnrtc;
using namespace ndn;
using namespace boost::chrono;
using namespace ndnrtc::statistics;

TEST(TestAudioPlayout, TestG722)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});
	boost::asio::deadline_timer runTimer(io);

	int runTime = 5000;
	int oneWayDelay = 100; // milliseconds
	int deviation = 50;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 40;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)targetSize/(double)samplePeriod)+1;	
	MockAudioThreadCallback callback;
	AudioThreadParams ap("hd", "g722");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	boost::shared_ptr<AudioBundlePacket> bundle(boost::make_shared<AudioBundlePacket>(wire_length));
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;
	uint64_t bundleNo = 0;
	high_resolution_clock::time_point callbackTs;

	DelayQueue queue(io, oneWayDelay, deviation);
	DataCache cache;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd";

	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(pipeline*5)); // make sure we re-use slots
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	
	MockPlayoutObserver playoutObserver;
	AudioPlayout playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setLogger(&ndnlog::new_api::Logger::getLogger(""));
	at.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(&ndnlog::new_api::Logger::getLogger(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [&callbackTs, wire_length,
	 &bundle, &nBundles, &bundleNo, &cache, &queue, threadPrefix](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		CommonHeader hdr;
		hdr.sampleRate_ = 25.;
		hdr.publishTimestampMs_ = clock::millisecondTimestamp();
		hdr.publishUnixTimestamp_ = clock::unixTimestamp();
		bundle->setHeader(hdr);

		std::vector<CommonSegment> segments = CommonSegment::slice(*bundle, 1000);
		int idx = 0;
		for (auto& s:segments)
		{
			boost::shared_ptr<NetworkData> segmentData = s.getNetworkData();
			Name segmentName(threadPrefix);
			segmentName.appendSequenceNumber(nBundles).appendSegment(idx);
			boost::shared_ptr<ndn::Data> d(boost::make_shared<ndn::Data>(segmentName));
			d->getMetaInfo().setFreshnessPeriod(1000);
			d->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size()-1));
			d->setContent(segmentData->getData(), segmentData->getLength());
			cache.addData(d);
			idx++;

#ifdef ENABLE_LOGGING
			LogDebug("") << "published " << d->getName() << std::endl;
#endif
		}

		nBundles++;
	};
	EXPECT_CALL(callback, onSampleBundle("hd",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));

	boost::function<void(boost::shared_ptr<WireData<DataSegmentHeader>>)> onDataArrived;
	boost::function<void(PacketNumber pno)> requestFrame = [&queue, &cache, buffer, 
		&onDataArrived, threadPrefix]
		(PacketNumber pno)
	{
		Name frameName(threadPrefix);
		frameName.appendSequenceNumber(pno).appendSegment(0);

		boost::shared_ptr<ndn::Interest> i(boost::make_shared<ndn::Interest>(frameName,1000));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		i->setNonce(Blob((uint8_t*)&pno, sizeof(PacketNumber)));
#pragma GCC diagnostic pop

		std::vector<boost::shared_ptr<ndn::Interest>> interests;
		interests.push_back(i);
		buffer->requested(makeInterestsConst(interests));

#ifdef ENABLE_LOGGING
		LogDebug("") << "express " << i->getName() << std::endl;
#endif

		queue.push([i, &cache, &queue, buffer, &onDataArrived](){
			cache.addInterest(i, [&queue, buffer, &onDataArrived](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
				queue.push([buffer, &onDataArrived, d,i ](){
#ifdef ENABLE_LOGGING
					LogDebug("") << "received " << d->getName() << std::endl;
#endif
					boost::shared_ptr<WireData<DataSegmentHeader>> data = 
						boost::make_shared<WireData<DataSegmentHeader>>(d, i);
					onDataArrived(data);
					BufferReceipt r = buffer->received(data);
				});
			});
		});
	};

	int nRequested = 0;
	onDataArrived = [&requestFrame, &nRequested]
		(boost::shared_ptr<WireData<DataSegmentHeader>> data)
	{
		requestFrame(nRequested);
		nRequested++;
	};

	for (nRequested = 0; nRequested < pipeline; ++nRequested)
		requestFrame(nRequested);

	int queuSizeAccum = 0;
	int nQueueSize = 0;
	boost::atomic<bool> done(false);
	EXPECT_CALL(pobserver, onNewSampleReady())
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&done, &playout, targetSize, pqueue, 
			&queuSizeAccum, &nQueueSize]()
			{ 
				if (!done && pqueue->size() >= targetSize && !playout.isRunning())
					playout.start();
				queuSizeAccum += pqueue->size();
				nQueueSize++;
			}));

	int drainCount = 0;
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&drainCount](){ 
			drainCount++;
		}));


	at.start();
	
	runTimer.expires_from_now(boost::posix_time::milliseconds(runTime));
	runTimer.wait();

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(1)
		.WillOnce(Invoke([&playout, &done, &t, &work](){ 
			done = true;
			playout.stop();
			work.reset();
		}));

	at.stop();
	queue.reset();
	t.join();

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::releaseAsyncLogging();
#endif

	GT_PRINTF("Queue drain count %d, Avg play queue size: %.2fms (target %dms)\n", 
		drainCount, (double)queuSizeAccum/(double)nQueueSize, targetSize);
}

TEST(TestAudioPlayout, TestOpus)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});
	boost::asio::deadline_timer runTimer(io);

	int runTime = 5000;
	int oneWayDelay = 100; // milliseconds
	int deviation = 50;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 40;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)targetSize/(double)samplePeriod)+1;	
	MockAudioThreadCallback callback;
	AudioThreadParams ap("hd", "opus");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	boost::shared_ptr<AudioBundlePacket> bundle(boost::make_shared<AudioBundlePacket>(wire_length));
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;
	uint64_t bundleNo = 0;
	high_resolution_clock::time_point callbackTs;

	DelayQueue queue(io, oneWayDelay, deviation);
	DataCache cache;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd";

	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(pipeline*5)); // make sure we re-use slots
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	
	MockPlayoutObserver playoutObserver;
	AudioPlayout playout(io, pqueue, boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()),
	 WebrtcAudioChannel::Codec::Opus, 0);

#ifdef ENABLE_LOGGING
	playout.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(&ndnlog::new_api::Logger::getLogger(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [&callbackTs, wire_length,
	 &bundle, &nBundles, &bundleNo, &cache, &queue, threadPrefix](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		CommonHeader hdr;
		hdr.sampleRate_ = 25.;
		hdr.publishTimestampMs_ = clock::millisecondTimestamp();
		hdr.publishUnixTimestamp_ = clock::unixTimestamp();
		bundle->setHeader(hdr);

		std::vector<CommonSegment> segments = CommonSegment::slice(*bundle, 1000);
		int idx = 0;
		for (auto& s:segments)
		{
			boost::shared_ptr<NetworkData> segmentData = s.getNetworkData();
			Name segmentName(threadPrefix);
			segmentName.appendSequenceNumber(nBundles).appendSegment(idx);
			boost::shared_ptr<ndn::Data> d(boost::make_shared<ndn::Data>(segmentName));
			d->getMetaInfo().setFreshnessPeriod(1000);
			d->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size()-1));
			d->setContent(segmentData->getData(), segmentData->getLength());
			cache.addData(d);
			idx++;

#ifdef ENABLE_LOGGING
			LogDebug("") << "published " << d->getName() << std::endl;
#endif
		}

		nBundles++;
	};
	EXPECT_CALL(callback, onSampleBundle("hd",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));

	boost::function<void(boost::shared_ptr<WireData<DataSegmentHeader>>)> onDataArrived;
	boost::function<void(PacketNumber pno)> requestFrame = [&queue, &cache, buffer, 
		&onDataArrived, threadPrefix]
		(PacketNumber pno)
	{
		Name frameName(threadPrefix);
		frameName.appendSequenceNumber(pno).appendSegment(0);

		boost::shared_ptr<ndn::Interest> i(boost::make_shared<ndn::Interest>(frameName,1000));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		i->setNonce(Blob((uint8_t*)&pno, sizeof(PacketNumber)));
#pragma GCC diagnostic pop

		std::vector<boost::shared_ptr<ndn::Interest>> interests;
		interests.push_back(i);
		buffer->requested(makeInterestsConst(interests));

#ifdef ENABLE_LOGGING
		LogDebug("") << "express " << i->getName() << std::endl;
#endif
        
		queue.push([i, &cache, &queue, buffer, &onDataArrived](){
			cache.addInterest(i, [&queue, buffer, &onDataArrived](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
				queue.push([buffer, &onDataArrived, d, i](){

#ifdef ENABLE_LOGGING
					LogDebug("") << "received " << d->getName() << std::endl;
#endif
					boost::shared_ptr<WireData<DataSegmentHeader>> data = 
						boost::make_shared<WireData<DataSegmentHeader>>(d, i);
					onDataArrived(data);
					BufferReceipt r = buffer->received(data);
				});
			});
		});
	};

	int nRequested = 0;
	onDataArrived = [&requestFrame, &nRequested]
		(boost::shared_ptr<WireData<DataSegmentHeader>> data)
	{
		requestFrame(nRequested);
		nRequested++;
	};

	for (nRequested = 0; nRequested < pipeline; ++nRequested)
		requestFrame(nRequested);

	int queuSizeAccum = 0;
	int nQueueSize = 0;
	boost::atomic<bool> done(false);
	EXPECT_CALL(pobserver, onNewSampleReady())
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&done, &playout, targetSize, pqueue, 
			&queuSizeAccum, &nQueueSize]()
			{ 
				if (!done && pqueue->size() >= targetSize && !playout.isRunning())
					playout.start();
				queuSizeAccum += pqueue->size();
				nQueueSize++;
			}));

	int drainCount = 0;
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&drainCount](){ 
			drainCount++;
		}));


	at.start();
	
	runTimer.expires_from_now(boost::posix_time::milliseconds(runTime));
	runTimer.wait();

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(1)
		.WillOnce(Invoke([&playout, &done, &t, &work](){ 
			done = true;
			playout.stop();
			work.reset();
		}));

	at.stop();
	queue.reset();
	t.join();


	GT_PRINTF("Queue drain count %d, Avg play queue size: %.2fms (target %dms)\n", 
		drainCount, (double)queuSizeAccum/(double)nQueueSize, targetSize);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
