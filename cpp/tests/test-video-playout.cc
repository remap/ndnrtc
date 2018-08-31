// 
// test-video-playout.cc
//
//  Created by Peter Gusev on 17 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/assign.hpp>

#include "gtest/gtest.h"
#include "src/video-playout.hpp"
#include "src/frame-buffer.hpp"
#include "tests-helpers.hpp"
#include "name-components.hpp"
#include "async.hpp"
#include "client/src/video-source.hpp"
#include "src/video-thread.hpp"
#include "src/frame-converter.hpp"
#include "src/clock.hpp"
#include "statistics.hpp"

#include "mock-objects/buffer-observer-mock.hpp"
#include "mock-objects/playback-queue-observer-mock.hpp"
#include "mock-objects/playout-observer-mock.hpp"
#include "mock-objects/external-capturer-mock.hpp"
#include "mock-objects/video-playout-consumer-mock.hpp"

// #define ENABLE_LOGGING
#if 1
using namespace testing;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

std::string test_path = "";

//******************************************************************************
TEST(TestPlayout, TestPlayout100msDelay)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#else
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int runTimeMs = 3000;
	int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
	int nPublished(0), nPublishedKey(0), nPublishedDelta(0); 
	int oneWayDelay = 100; // milliseconds
	int deviation = 0;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 22;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)targetSize/(double)samplePeriod)+1;
	DelayQueue queue(io, oneWayDelay, deviation);
	DataCache cache;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(320,240));
	std::string testVideoSource = test_path+"/../../res/test-source-320x240.argb";
	VideoSource source(io, testVideoSource, frame);
	MockExternalCapturer capturer;
	source.addCapturer(&capturer);
	
	VideoCoderParams cp = sampleVideoCoderParams();
	cp.codecFrameRate_ = captureFps;
	cp.startBitrate_ = 200;
	cp.encodeWidth_ = 320;
	cp.encodeHeight_ = 240;

	VideoThread vthread(cp);
	RawFrameConverter fc;
	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(pipeline*5)); // make sure we re-use slots
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	
	MockVideoPlayoutObserver playoutObserver;
	VideoPlayout playout(io, pqueue);

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

	int publishStart = 0, lastPublishTime = 0;
	boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
		publishFrame = [captureFps, threadPrefix, &cache, &vthread, &fc, &nPublished, pipeline,
				&nPublishedKey, &nPublishedDelta, &publishStart, &lastPublishTime]
			(const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size)
		{
			boost::shared_ptr<VideoFramePacket> vp = vthread.encode(fc << ArgbRawFrameWrapper({w,h,data,size}));
			if (vp.get())
			{
				std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);
				vp->setSyncList(syncList);

				CommonHeader hdr;
				hdr.sampleRate_ = captureFps;
				hdr.publishTimestampMs_ = clock::millisecondTimestamp();
				hdr.publishUnixTimestamp_ = clock::unixTimestamp();
				vp->setHeader(hdr);

				if (publishStart == 0)
					publishStart = hdr.publishTimestampMs_;
				lastPublishTime = hdr.publishTimestampMs_;

				bool isKey = vp->getFrame()._frameType == webrtc::kVideoFrameKey;
				int paired = (isKey ? nPublishedDelta : nPublishedKey);
				int& seq = (isKey ? nPublishedKey : nPublishedDelta);
				std::vector<ndnrtc::VideoFrameSegment> segments = sliceFrame(*vp, nPublished++, paired);
				Name frameName(threadPrefix);
				frameName.append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
					.appendSequenceNumber(seq);
				std::vector<boost::shared_ptr<ndn::Data>> objects = dataFromSegments(frameName.toUri(), segments);
				
				seq++;

				// simulate random order delivery
				std::random_shuffle(objects.begin(), objects.end());

				for (auto& d:objects)
					cache.addData(d);
#ifdef ENABLE_LOGGING
					LogDebug("") << "published " << frameName << " " << frameName[-1].toSequenceNumber() << std::endl;
#endif
			}
			return 0;
		};
	EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
		.WillRepeatedly(Invoke(publishFrame));

	boost::function<void(boost::shared_ptr<WireData<VideoFrameSegmentHeader>>)> onDataArrived;
	boost::function<void(PacketNumber, bool)> 
		requestFrame = [threadPrefix, &queue, &cache, buffer, &onDataArrived]
			(PacketNumber fno, bool key)
		{
			int nSeg = (key ? 20 : 10);
			Name frameName(threadPrefix);
			
			if (key) frameName.append(NameComponents::NameComponentKey);
			else frameName.append(NameComponents::NameComponentDelta);
			
			frameName.appendSequenceNumber(fno);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			ASSERT_TRUE(buffer->requested(makeInterestsConst(interests)));

			for (auto& i:interests)
			{
				// send out interest
				queue.push([&queue, &cache, i, buffer, &onDataArrived](){
					// when received on producer side - add interest to cache
					cache.addInterest(i, [&queue, buffer, &onDataArrived](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
						ASSERT_EQ(d->getName(), i->getName());

						// when data becomes available - send it back
						queue.push([buffer, d, &onDataArrived, i](){
							// when data arrives - add to buffer
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);
							onDataArrived(data);
							BufferReceipt r = buffer->received(data);
						});
					});
				});
			}
		};

	onDataArrived = [&requestFrame, &nRequested, &nRequestedDelta, &nRequestedKey, pipeline]
			(boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data)
		{
			int& seqNo = (data->isDelta() ? nRequestedDelta : nRequestedKey);
			int pp = (data->isDelta() ? pipeline : 1);
			if (data->getSampleNo()+pp >= seqNo)
			{
				requestFrame(seqNo++, !data->isDelta());
				++nRequested;
			}
		};

	int pqueueSizeCnt = 0;
	int pqueueSize = 0;
	bool done = false;
	boost::function<void(void)> 
		sampleReady = [&done, 
						cp, targetSize, samplePeriod, pqueue, &playout, 
						&pqueueSizeCnt, &pqueueSize]()
		{
			pqueueSizeCnt++;
			pqueueSize += pqueue->size();

			if (pqueue->size() >= targetSize && !playout.isRunning() && !done)
				EXPECT_NO_THROW(playout.start());
		};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.WillRepeatedly(Invoke(sampleReady));

	int nPlayed = 0;
	boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
	int emptyCount = 0;
	int nProcessedDelta = 0, nProcessedKey = 0;
	int nSkippedDelta = 0, nSkippedKey = 0;
	int nRecoveryFailures = 0;

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&emptyCount]() {
			emptyCount++; 
		}));
	EXPECT_CALL(playoutObserver, frameProcessed(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&nProcessedKey, &nProcessedDelta, &nPlayed, &lastTimestamp, &startPlayback]
			(PacketNumber pno, bool isKey)
		{
			if (isKey) ++nProcessedKey;
			else ++nProcessedDelta;

			boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
			lastTimestamp = p;

			if (startPlayback == boost::chrono::high_resolution_clock::time_point())
				startPlayback = p;
			nPlayed++;
		}));
	EXPECT_CALL(playoutObserver, frameSkipped(_,_))
		.Times(0);
	EXPECT_CALL(playoutObserver, recoveryFailure(_,_))
		.Times(0);

	// request initial frames
	requestFrame(nRequestedKey++, true);
	for (nRequested = 1; nRequested <= pipeline; ++nRequested)
	{
		requestFrame(nRequestedDelta, false);
		nRequestedDelta++;
	}

	source.start(captureFps);

	boost::this_thread::sleep_for(boost::chrono::milliseconds(runTimeMs));
	done = true;
	source.stop();
	queue.reset();

	boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay*2+4*deviation));
	playout.stop();

	work.reset();
	t.join();

	int playbackDuration = boost::chrono::duration_cast<boost::chrono::milliseconds>(lastTimestamp-startPlayback).count();
	int publishDuration = lastPublishTime - publishStart;
	double avgQueueSize = (double)pqueueSize/(double)pqueueSizeCnt;
	double avgPublishRate = (double)nPublished/((double)(publishDuration)/1000.);
	double avgPlayRate = (double)nPlayed/((double)playbackDuration/1000.);
	double avgPublishPeriod = (double)publishDuration/(double)nPublished;
	double avgPlayPeriod = (double)playbackDuration/(double)nPlayed;
	int tmp = nPublished;
	double queueSizeDeviation = 0.2;

	GT_PRINTF("Total published: %dms (%d frames), played: %dms (%d frames)\n",
		publishDuration, tmp, playbackDuration, nPlayed);
	GT_PRINTF("Average playout queue size %.2fms (%.2f frames); target %dms. Queue drain count: %d\n", 
		avgQueueSize, avgQueueSize/avgPlayPeriod, targetSize, emptyCount);
	GT_PRINTF("avg publish rate %.2f; avg play rate: %.2f; avg publish period: %2.f; avg play period: %.2f\n",
		avgPublishRate, avgPlayRate, avgPublishPeriod, avgPlayPeriod);

#ifdef ENABLE_LOGGING
			LogDebug("") << buffer->dump() << std::endl;
#endif

	EXPECT_GT(pipeline*samplePeriod, std::abs(playbackDuration-publishDuration));
	EXPECT_FALSE(source.isRunning());
	EXPECT_EQ(0, pqueue->size());
	EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
		BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	
	EXPECT_GT(1, std::abs(avgPlayPeriod-avgPublishPeriod)); 	// ~1ms error
	EXPECT_GT(0.1, std::abs(avgPlayRate-avgPublishRate));	// ~0.1 error
	
	ASSERT_FALSE(playout.isRunning());

	ndnlog::new_api::Logger::getLogger("").flush();
}
#endif
#if 1
//******************************************************************************
TEST(TestPlayout, TestSkipDelta)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#else
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int runTimeMs = 3000;
	int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
	int nPublished(0), nPublishedKey(0), nPublishedDelta(0); 
	int oneWayDelay = 100; // milliseconds
	int deviation = 0;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 22;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)targetSize/(double)samplePeriod)+1;
	DelayQueue queue(io, oneWayDelay, deviation);
	DataCache cache;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(320,240));
	std::string testVideoSource = test_path+"/../../res/test-source-320x240.argb";
	VideoSource source(io, testVideoSource, frame);
	MockExternalCapturer capturer;
	source.addCapturer(&capturer);
	
	VideoCoderParams cp = sampleVideoCoderParams();
	cp.codecFrameRate_ = captureFps;
	cp.startBitrate_ = 200;
	cp.encodeWidth_ = 320;
	cp.encodeHeight_ = 240;

	VideoThread vthread(cp);
	RawFrameConverter fc;
	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(pipeline*5)); // make sure we re-use slots
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	
	MockVideoPlayoutObserver playoutObserver;
	MockVideoPlayoutConsumer frameConsumer;
	VideoPlayout playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);
	playout.registerFrameConsumer(&frameConsumer);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(&ndnlog::new_api::Logger::getLogger(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	int gopCount = 0, nSkipped = 0;
	int publishStart = 0, lastPublishTime = 0;
	boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
		publishFrame = [captureFps, threadPrefix, &cache, &vthread, &fc, &nPublished, pipeline,
				&nPublishedKey, &nPublishedDelta, &publishStart, &lastPublishTime, 
				&gopCount, &nSkipped]
			(const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size)
		{
			boost::shared_ptr<VideoFramePacket> vp = vthread.encode(fc << ArgbRawFrameWrapper({w,h,data,size}));
			if (vp.get())
			{
				std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);
				vp->setSyncList(syncList);

				CommonHeader hdr;
				hdr.sampleRate_ = captureFps;
				hdr.publishTimestampMs_ = clock::millisecondTimestamp();
				hdr.publishUnixTimestamp_ = clock::unixTimestamp();
				vp->setHeader(hdr);

				if (publishStart == 0)
					publishStart = hdr.publishTimestampMs_;
				lastPublishTime = hdr.publishTimestampMs_;

				bool isKey = vp->getFrame()._frameType == webrtc::kVideoFrameKey;
				int paired = (isKey ? nPublishedDelta : nPublishedKey);
				int& seq = (isKey ? nPublishedKey : nPublishedDelta);
				std::vector<ndnrtc::VideoFrameSegment> segments = sliceFrame(*vp, nPublished++, paired);

				Name frameName(threadPrefix);
				frameName.append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
					.appendSequenceNumber(seq);
				std::vector<boost::shared_ptr<ndn::Data>> objects = dataFromSegments(frameName.toUri(), segments);

				std::vector<boost::shared_ptr<ndn::Data>> parityObjects;
				{
					boost::shared_ptr<NetworkData> parity;
					std::vector<ndnrtc::VideoFrameSegment> paritySegments = sliceParity(*vp, parity);
					parityObjects = dataFromParitySegments(frameName.toUri(), paritySegments);
				}

				
				if (isKey) ++gopCount;

				bool skip = (gopCount > 1 && !isKey && nSkipped == 0);
				seq++;
				int idx = 0;
				
				for (auto& d:objects)
				{
					if (skip && idx >= objects.size() - parityObjects.size())
					{
						LogDebug("") << "skip segment " << d->getName() << std::endl;
						nSkipped++;
					}
					else
						cache.addData(d);
					++idx;
				}

				for (auto& d:parityObjects)
					cache.addData(d);

#ifdef ENABLE_LOGGING
				if (!skip)
					LogDebug("") << "published " << frameName << " " << frameName[-1].toSequenceNumber() << std::endl;
#endif
			}
			return 0;
		};
	EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
		.WillRepeatedly(Invoke(publishFrame));

	int nInvalidated = 0;
	boost::function<void(boost::shared_ptr<WireSegment>)> onDataArrived;
	boost::function<void(PacketNumber, bool)> 
		requestFrame = [threadPrefix, &queue, &cache, buffer, &onDataArrived, &nInvalidated]
			(PacketNumber fno, bool key)
		{
			int nSeg = (key ? 20 : 10);
			Name frameName(threadPrefix);
			
			if (key) frameName.append(NameComponents::NameComponentKey);
			else frameName.append(NameComponents::NameComponentDelta);
			
			frameName.appendSequenceNumber(fno);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg, 0, 2);
			ASSERT_TRUE(buffer->requested(makeInterestsConst(interests)));
			
			// simulate random order delivery
			std::random_shuffle(interests.begin(), interests.end());

			for (auto& i:interests)
			{
				// send out interest
				queue.push([&queue, &cache, i, buffer, &onDataArrived, &nInvalidated](){
					// when received on producer side - add interest to cache
					cache.addInterest(i, [&queue, buffer, &onDataArrived, &nInvalidated](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
						// when data becomes available - send it back
						queue.push([buffer, d, &onDataArrived, i, &nInvalidated](){
							boost::shared_ptr<WireSegment> data;
							NamespaceInfo info;
							NameComponents::extractInfo(d->getName(), info);
                            data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);

							onDataArrived(data);
							if (buffer->isRequested(data))
								buffer->received(data);
							else 
								nInvalidated++;
						});
					});
				});
			}
		};

	onDataArrived = [&requestFrame, &nRequested, &nRequestedDelta, &nRequestedKey, pipeline]
			(boost::shared_ptr<WireSegment> data)
		{
			int& seqNo = (data->isDelta() ? nRequestedDelta : nRequestedKey);
			int pp = (data->isDelta() ? pipeline : 1);
			if (data->getSampleNo()+pp >= seqNo)
			{
				requestFrame(seqNo++, !data->isDelta());
				++nRequested;
			}
		};

	int pqueueSizeCnt = 0;
	int pqueueSize = 0;
	bool done = false;
	boost::function<void(void)> 
		sampleReady = [&done, 
						cp, targetSize, samplePeriod, pqueue, &playout, 
						&pqueueSizeCnt, &pqueueSize]()
		{
			pqueueSizeCnt++;
			pqueueSize += pqueue->size();

			if (pqueue->size() >= targetSize && !playout.isRunning() && !done)
				EXPECT_NO_THROW(playout.start());
		};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.WillRepeatedly(Invoke(sampleReady));

	int nPlayed = 0;
	boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
	int emptyCount = 0;
	int nProcessedDelta = 0, nProcessedKey = 0;
	int nSkippedDelta = 0, nSkippedKey = 0;
	int nRecoveryFailures = 0;

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&emptyCount]() {
			emptyCount++; 
		}));
	EXPECT_CALL(playoutObserver, frameProcessed(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke([&nProcessedKey, &nProcessedDelta, &nPlayed, &lastTimestamp, &startPlayback]
			(PacketNumber pno, bool isKey)
		{
			if (isKey) ++nProcessedKey;
			else ++nProcessedDelta;

			boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
			lastTimestamp = p;

			if (startPlayback == boost::chrono::high_resolution_clock::time_point())
				startPlayback = p;
			nPlayed++;
		}));
	EXPECT_CALL(playoutObserver, frameSkipped(_,_))
		.Times(AtLeast(25));

	int nEncodedReceived = 0;
	boost::function<void(const FrameInfo&, const webrtc::EncodedImage&)> processFrame = [&nEncodedReceived](const FrameInfo&, const webrtc::EncodedImage&){
		nEncodedReceived++;
	};
	EXPECT_CALL(frameConsumer, processFrame(_, _))
		.WillRepeatedly(Invoke(processFrame));

	// request initial frames
	requestFrame(nRequestedKey++, true);
	for (nRequested = 1; nRequested <= pipeline; ++nRequested)
	{
			requestFrame(nRequestedDelta, false);
			nRequestedDelta++;
	}

	source.start(captureFps);

	boost::this_thread::sleep_for(boost::chrono::milliseconds(runTimeMs));
	done = true;
	source.stop();
	queue.reset();

	boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay*2+4*deviation));
	playout.stop();
	work.reset();
	t.join();

	int playbackDuration = boost::chrono::duration_cast<boost::chrono::milliseconds>(lastTimestamp-startPlayback).count();
	int publishDuration = lastPublishTime - publishStart;
	double avgQueueSize = (double)pqueueSize/(double)pqueueSizeCnt;
	double avgPublishRate = (double)nPublished/((double)(publishDuration)/1000.);
	double avgPlayRate = (double)nPlayed/((double)playbackDuration/1000.);
	double avgPublishPeriod = (double)publishDuration/(double)nPublished;
	double avgPlayPeriod = (double)playbackDuration/(double)nPlayed;
	int tmp = nPublished;
	double queueSizeDeviation = 0.2;

	GT_PRINTF("Total published: %dms (%d frames), played: %dms (%d frames)\n",
		publishDuration, tmp, playbackDuration, nPlayed);
	GT_PRINTF("Average playout queue size %.2fms (%.2f frames); target %dms. Queue drain count: %d\n", 
		avgQueueSize, avgQueueSize/avgPlayPeriod, targetSize, emptyCount);
	GT_PRINTF("avg publish rate %.2f; avg play rate: %.2f; avg publish period: %2.f; avg play period: %.2f\n",
		avgPublishRate, avgPlayRate, avgPublishPeriod, avgPlayPeriod);

#ifdef ENABLE_LOGGING
			LogDebug("") << buffer->dump() << std::endl;
#endif

	EXPECT_GT(pipeline*samplePeriod, std::abs(playbackDuration-publishDuration));
	EXPECT_FALSE(source.isRunning());
	ASSERT_FALSE(playout.isRunning());

	ndnlog::new_api::Logger::getLogger("").flush();
}
#endif

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	
	test_path = std::string(argv[0]);
	std::vector<std::string> comps;
    boost::split(comps, test_path, boost::is_any_of("/"));

    test_path = "";
    for (int i = 0; i < comps.size()-1; ++i) 
    {
    	test_path += comps[i];
    	if (i != comps.size()-1) test_path += "/";
    }

	return RUN_ALL_TESTS();
}

