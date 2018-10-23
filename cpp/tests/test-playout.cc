// 
// test-playout.cc
//
//  Created by Peter Gusev on 02 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <cstdlib>
#include <ctime>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/assign.hpp>

#include "gtest/gtest.h"
#include "src/frame-buffer.hpp"
#include "src/playout.hpp"
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

// #define ENABLE_LOGGING

using namespace testing;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

std::string test_path = "";

class PlayoutTestImpl : public PlayoutImpl
{
public:
	PlayoutTestImpl(boost::asio::io_service& io,
        const boost::shared_ptr<IPlaybackQueue>& queue,
        const boost::shared_ptr<statistics::StatisticsStorage> statStorage=
        boost::shared_ptr<statistics::StatisticsStorage>(StatisticsStorage::createConsumerStatistics())):
	PlayoutImpl(io, queue, statStorage){}

	boost::function<void(const boost::shared_ptr<const BufferSlot>& slot)> onSampleReadyForPlayback;

private:
	bool processSample(const boost::shared_ptr<const BufferSlot>& slot){
		onSampleReadyForPlayback(slot);
        return true;
	}
};

class PlayoutTest : public Playout
{
public:
	PlayoutTest(boost::asio::io_service& io,
		const boost::shared_ptr<PlaybackQueue>& queue):
	Playout(boost::make_shared<PlayoutTestImpl>(io, queue)){}
	PlayoutTestImpl* pimpl(){ return (PlayoutTestImpl*)Playout::pimpl(); }
};
#if 1
TEST(TestPlaybackQueue, TestAttach)
{
    int nSamples = 100;
    int delay = 10; // samples delay between data request and data produce
    int targetSize = 150;
    boost::atomic<int> sampleNo(0);
    double fps = 30;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera";
    std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    
    MockBufferObserver bobserver;
    MockPlaybackQueueObserver pobserver;
    boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(delay*3)); // make sure we re-use slots
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
        boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
    boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
    
    EXPECT_NO_THROW(pqueue->detach(nullptr));
    EXPECT_NO_THROW(pqueue->attach(nullptr));
    EXPECT_NO_THROW(pqueue->detach(nullptr));
    EXPECT_NO_THROW(pqueue->attach(&pobserver));
    EXPECT_NO_THROW(pqueue->attach(nullptr));
    EXPECT_NO_THROW(pqueue->detach(nullptr));
}
#endif
TEST(TestPlaybackQueue, TestPlay)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif
    
	int nSamples = 100;
	int delay = 10; // samples delay between data request and data produce
	int targetSize = 150;
	boost::atomic<int> sampleNo(0);
	double fps = 30;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	
	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(delay*3)); // make sure we re-use slots
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
        boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	
	buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
    
#ifdef ENABLE_LOGGING
    buffer->setDescription("buffer");
    buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    pqueue->setDescription("pqueue");
    pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	boost::thread requestor([&sampleNo, threadPrefix, buffer, nSamples, fps](){
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		for (sampleNo = 0; sampleNo < nSamples; ++sampleNo)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds((int)(1000./fps)));

			int nSeg = (sampleNo%((int)fps) == 0 ? 30: 10);
			Name frameName(threadPrefix);
			if (sampleNo%30 == 0)
			{
				frameName.append(NameComponents::NameComponentKey);
			}
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(sampleNo);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			EXPECT_TRUE(buffer->requested(makeInterestsConst(interests)));
			runTimer.wait();
		}
	});

	boost::thread provider([&sampleNo, delay, threadPrefix, buffer, nSamples, fps](){
		while (sampleNo < delay) ;
		int64_t ts = 488589553, uts = 1460488589;
		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		for (int n = 0; n < nSamples; ++n){
			runTimer.expires_from_now(boost::posix_time::milliseconds((int)(1000./fps)));

			Name frameName(threadPrefix);
			if (n%30 == 0)
				frameName.append(NameComponents::NameComponentKey);
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(n);

			VideoFramePacket vp = getVideoFramePacket((n%(int)fps == 0 ? 28000 : 8000),
				fps, ts+n*(int)(1000./fps), uts+n*(int)(1000./fps));
			std::vector<VideoFrameSegment> segments = sliceFrame(vp);
			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, segments.size());
			std::vector<boost::shared_ptr<Data>> data = dataFromSegments(frameName.toUri(), segments);

			int idx = 0;
			for (auto d:data)
				buffer->received(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx++]));

			runTimer.wait();
		}
	});

	boost::asio::io_service consumerIo;
	boost::shared_ptr<boost::asio::io_service::work> consumerWork(new boost::asio::io_service::work(consumerIo));
	boost::thread consumer([&consumerIo](){
		consumerIo.run();
	});

	int lastFrameNo = -1;
	VideoFrameSlot vs;
	boost::function<void(void)> onSampleReady = [&consumerIo, &lastFrameNo, targetSize, delay, fps, buffer, pqueue, &vs](){
		if (pqueue->size() >= targetSize)
		{
			async::dispatchAsync(consumerIo, [&lastFrameNo, &vs, pqueue, fps, targetSize](){
				pqueue->pop([&vs, &lastFrameNo](const boost::shared_ptr<const BufferSlot>& slot, double playTimeMs){
					bool recovered;
					boost::shared_ptr<ImmutableVideoFramePacket> videoPacket = vs.readPacket(*slot, recovered);
					EXPECT_TRUE(videoPacket.get());
					EXPECT_TRUE(checkVideoFrame(videoPacket->getFrame()));
					EXPECT_LT(lastFrameNo, slot->getNameInfo().sampleNo_);
					lastFrameNo = slot->getNameInfo().sampleNo_;
				});
				EXPECT_GT(targetSize+(int)round(1000./fps), pqueue->size());
				EXPECT_LT(targetSize-(int)round(1000./fps), pqueue->size());
			});
		}

		// GT_PRINTF("buffer play level %ld, pending level %ld\n", pqueue->size(), pqueue->pendingSize());
		EXPECT_GT((delay+1)*(int)round(1000./fps), pqueue->pendingSize());
	};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.Times(nSamples)
		.WillRepeatedly(Invoke(onSampleReady));
	EXPECT_CALL(bobserver, onNewRequest(_))
		.Times(nSamples);
	EXPECT_CALL(bobserver, onNewData(_))
		.Times(AtLeast(8*nSamples));

	requestor.join();
	provider.join();
	consumerWork.reset();
	consumer.join();
}
#if 1
TEST(TestPlayout, TestPlay)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

	double fps = 30;
	int nSamples = 300;
	int samplePeriod = (int)(1000./fps);
	boost::atomic<int> pipeline(10);
	int rtt = 100;
	int targetSize = 150;
	boost::atomic<int> sampleNo(0);
	boost::interprocess::interprocess_semaphore sem(pipeline);
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera";
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	
	MockBufferObserver bobserver;
	MockPlaybackQueueObserver pobserver;
	boost::shared_ptr<SlotPool> pool(boost::make_shared<SlotPool>(pipeline*5)); // make sure we re-use slots
	boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
	    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage, pool));
	boost::shared_ptr<PlaybackQueue> pqueue(boost::make_shared<PlaybackQueue>(Name(streamPrefix), buffer));
	buffer->attach(&bobserver);
	pqueue->attach(&pobserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	boost::thread requestor([&sampleNo, &sem, &pipeline, threadPrefix, buffer, nSamples, samplePeriod, fps](){
		for (sampleNo = 0; sampleNo < nSamples; ++sampleNo)
		{
			int nSeg = (sampleNo%((int)fps) == 0 ? 30: 10);
			Name frameName(threadPrefix);
			if (sampleNo%30 == 0)
			{
				frameName.append(NameComponents::NameComponentKey);
			}
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(sampleNo);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			EXPECT_TRUE(buffer->requested(makeInterestsConst(interests)));

			sem.wait();
		}
	});

	double actualFps = 0;
	std::vector<int64_t> timestamps;
	boost::thread provider([rtt, &sampleNo, &actualFps, samplePeriod, threadPrefix, buffer, nSamples, fps, &timestamps](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(rtt));
		int64_t ts = 488589553, uts = 1460488589;
		boost::asio::io_service io;
		boost::asio::steady_timer runTimer(io);
		boost::chrono::high_resolution_clock::time_point start, lastTimestamp;
		int lastFno = 0;
		int timeResidueUsec = 0;

		for (int n = 0; n < nSamples; ++n){
			boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
			runTimer.expires_from_now(lib_chrono::milliseconds(samplePeriod));

			Name frameName(threadPrefix);
			if (n%30 == 0)
				frameName.append(NameComponents::NameComponentKey);
			else
				frameName.append(NameComponents::NameComponentDelta);
			frameName.appendSequenceNumber(n);

			if (n > 0)
			{
				boost::chrono::milliseconds dm = boost::chrono::duration_cast<boost::chrono::milliseconds>(p -lastTimestamp);
				boost::chrono::microseconds du = boost::chrono::duration_cast<boost::chrono::microseconds>(p -lastTimestamp);
				timeResidueUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(du-dm).count();
				
				int accumulatedResidueMs = 0;
				if (timeResidueUsec > 1000)
				{
					accumulatedResidueMs = timeResidueUsec/1000;
					timeResidueUsec -= accumulatedResidueMs*1000;
				}

				ts += dm.count() + accumulatedResidueMs;
				uts += dm.count() + accumulatedResidueMs;
			}

			VideoFramePacket vp = getVideoFramePacket((n%(int)fps == 0 ? 28000 : 8000),
				fps, ts, uts);
			timestamps.push_back(ts);

			std::vector<VideoFrameSegment> segments = sliceFrame(vp);
			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, segments.size());
			std::vector<boost::shared_ptr<Data>> data = dataFromSegments(frameName.toUri(), segments);

			int idx = 0;
			for (auto d:data)
				buffer->received(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, interests[idx++]));

			if (boost::chrono::duration_cast<boost::chrono::milliseconds>(p-start).count() >= 1000)
			{
				actualFps = (double)(n-lastFno)/(double)boost::chrono::duration_cast<boost::chrono::milliseconds>(p-start).count()*1000.;
				lastFno = n;
				start = p;
				// GT_PRINTF("Actual FPS %.2f\n", actualFps);
			}
			lastTimestamp = p;
			runTimer.wait();
		}
	});

	boost::asio::io_service consumerIo;
	boost::shared_ptr<boost::asio::io_service::work> consumerWork(new boost::asio::io_service::work(consumerIo));
	boost::thread consumer([&consumerIo](){
		consumerIo.run();
	});

	int frameNo = 0;
	VideoFrameSlot vs;
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(consumerIo, pqueue);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	std::vector<boost::chrono::high_resolution_clock::time_point> playbackTimestamps;
	boost::chrono::high_resolution_clock::time_point lastTimestamp, start;
	double playFps = 0;
	int lastFno = 0;
	playout.pimpl()->onSampleReadyForPlayback = [&lastTimestamp, &fps, &playFps, &lastFno, &start, &frameNo, &vs, &timestamps, &playbackTimestamps, &pqueue](const boost::shared_ptr<const BufferSlot>& slot){
		boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
		frameNo++;

		if (lastTimestamp != boost::chrono::high_resolution_clock::time_point())
		{
			bool recovered = false;
			boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
			EXPECT_TRUE(framePacket.get());
		}
		else
			start = p;

		lastTimestamp = p;
		playbackTimestamps.push_back(lastTimestamp);

		if (boost::chrono::duration_cast<boost::chrono::milliseconds>(p-start).count() >= 1000)
		{
			playFps = (double)(frameNo-lastFno)/(double)boost::chrono::duration_cast<boost::chrono::milliseconds>(lastTimestamp-start).count()*1000.;
			lastFno = frameNo;
			start = lastTimestamp;
			// GT_PRINTF("Playout FPS %.2f\n", playFps);
		}
	};

	boost::atomic<bool> enough(false);
	int lastFrameNo = -1;
	boost::function<void(void)> onSampleReady = [&sem, &pipeline, targetSize, samplePeriod, pqueue, fps, &playout, &enough](){
		if (pqueue->size() >= targetSize && !playout.isRunning() && !enough)
			EXPECT_NO_THROW(playout.start());
		
		if (playout.isRunning()) 
			EXPECT_LE(targetSize-1.5*samplePeriod, pqueue->size());

		sem.post();
	};

	boost::function<void(void)> queueEmpty = [&enough, &consumerWork, &consumerIo, &playout, &frameNo, nSamples](){
		enough = true;
		playout.stop();
		ASSERT_FALSE(playout.isRunning());
		consumerWork.reset();
		EXPECT_EQ(nSamples, frameNo);
	};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.Times(nSamples)
		.WillRepeatedly(Invoke(onSampleReady));
	EXPECT_CALL(bobserver, onNewRequest(_))
		.Times(nSamples);
	EXPECT_CALL(bobserver, onNewData(_))
		.Times(AtLeast(8*nSamples));
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(1)
		.WillOnce(Invoke(queueEmpty));

	provider.join();
	requestor.join();
	consumerWork.reset();
	consumer.join();

	EXPECT_EQ(timestamps.size(), playbackTimestamps.size());
	int pubDuration = timestamps.back() - timestamps.front();
	int playDuration = (boost::chrono::duration_cast<boost::chrono::milliseconds>(playbackTimestamps.back() - playbackTimestamps.front())).count();
	EXPECT_GT(samplePeriod, (int)std::abs((double)pubDuration-(double)playDuration) );
	
	GT_PRINTF("Actual FPS: %.2f, Playout FPS: %.2f\n", actualFps, playFps);
	EXPECT_GT(1, actualFps-playFps);
	EXPECT_EQ(nSamples, frameNo);
}
#endif
#if 1
TEST(TestPlayout, TestRequestAndPlayWithDelay)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	boost::atomic<int> request(0), requestKey(0), requestDelta(0), 
		published(0), publishedKey(0), publishedDelta(0); 
	int oneWayDelay = 150; // milliseconds
	int targetSize = oneWayDelay;
	int pipeline = 10;
	double captureFps = 27, playFps = 0.;
	int samplePeriod = (int)(1000./captureFps);
	DelayQueue queue(io, oneWayDelay);
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
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
		incomingRawFrame = [captureFps, threadPrefix, &cache, &vthread, &fc, &published, &publishedKey, &publishedDelta]
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

				bool isKey = vp->getFrame()._frameType == webrtc::kVideoFrameKey;
				int paired = (isKey ? publishedDelta : publishedKey);
				int seq = (isKey ? publishedKey : publishedDelta);
				std::vector<ndnrtc::VideoFrameSegment> segments = sliceFrame(*vp, published++, paired);
				Name frameName(threadPrefix);
				frameName.append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
					.appendSequenceNumber(seq);
				std::vector<boost::shared_ptr<ndn::Data>> objects = dataFromSegments(frameName.toUri(), segments);
				
				if (isKey) publishedKey++;
				else publishedDelta++;

				for (auto& d:objects)
				{
					// std::cout << "add " << d->getName() << std::endl;
					cache.addData(d);
				}

				LogDebug("") << "published " << frameName << " " << published << std::endl;
			}
			return 0;
		};
	EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
		.WillRepeatedly(Invoke(incomingRawFrame));

	boost::function<void(PacketNumber, bool)> requestFrame = [threadPrefix, &queue, &cache, buffer](PacketNumber fno, bool key){
			int nSeg = (key ? 10: 10);
			Name frameName(threadPrefix);
			
			if (key) frameName.append(NameComponents::NameComponentKey);
			else frameName.append(NameComponents::NameComponentDelta);
			
			frameName.appendSequenceNumber(fno);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			ASSERT_TRUE(buffer->requested(makeInterestsConst(interests)));
			
			for (auto& i:interests)
			{
				queue.push([&queue, &cache, i, buffer](){
					cache.addInterest(i, [&queue, buffer](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
						queue.push([buffer, d, i](){
							BufferReceipt r = buffer->received(boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i));
						});
					});
				});
			}
	};

	boost::function<void(void)> onSampleReady = [&request, &requestKey, &requestDelta, &requestFrame, cp, targetSize, samplePeriod, pqueue, &playout](){
		if (pqueue->size() >= targetSize && !playout.isRunning())
			EXPECT_NO_THROW(playout.start());
		
		if (playout.isRunning()) 
			EXPECT_LE(targetSize-1.5*samplePeriod, pqueue->size());

		if (request%cp.gop_ == 0)
			requestFrame(requestKey++, true);
		else
			requestFrame(requestDelta++, false);
		request++;
	};
	EXPECT_CALL(pobserver, onNewSampleReady())
		.WillRepeatedly(Invoke(onSampleReady));

	int nPlayed = 0;
	VideoFrameSlot vs;
	playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed](const boost::shared_ptr<const BufferSlot>& slot){
		bool recovered = false;
		boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
		EXPECT_TRUE(framePacket.get());
		nPlayed++;
	};

	boost::function<void(void)> queueEmpty = [pipeline, &playout, &source, &pqueue, &buffer, streamPrefix, &nPlayed, &request, &published](){
		playout.stop();

		EXPECT_FALSE(source.isRunning());
		EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
		EXPECT_EQ(pipeline, request-nPlayed);
		EXPECT_EQ(nPlayed, published);
		EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
			BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
		EXPECT_EQ(request-nPlayed, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::New));
		EXPECT_GE(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
		EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
		ASSERT_FALSE(playout.isRunning());
	};

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(1)
		.WillOnce(Invoke(queueEmpty));

	// request initial frames
	for (request = 0; request < pipeline; ++request)
	{
		requestFrame((request%cp.gop_ ? requestDelta : requestKey), request%cp.gop_ == 0);
		if (request%cp.gop_ == 0) requestKey++;
		else requestDelta++;
	}

	source.start(captureFps);

	boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
	source.stop();

	work.reset();
	t.join();
}
#endif
#if 1
TEST(TestPlayout, TestRequestAndPlayWithDeviation)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	boost::atomic<int> request(0), requestKey(0), requestDelta(0), 
		published(0), publishedKey(0), publishedDelta(0); 
	int oneWayDelay = 75; // milliseconds
	int deviation = 50;
	int targetSize = oneWayDelay; //+deviation;
	int pipeline = 10;
	double captureFps = 27, playFps = 0.;
	int samplePeriod = (int)(1000./captureFps);
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
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
		publishFrame = [captureFps, threadPrefix, &cache, &vthread, &fc, &published, &publishedKey, &publishedDelta]
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

				bool isKey = vp->getFrame()._frameType == webrtc::kVideoFrameKey;
				int paired = (isKey ? publishedDelta : publishedKey);
				int seq = (isKey ? publishedKey : publishedDelta);
				std::vector<ndnrtc::VideoFrameSegment> segments = sliceFrame(*vp, published++, paired);
				Name frameName(threadPrefix);
				frameName.append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
					.appendSequenceNumber(seq);
				std::vector<boost::shared_ptr<ndn::Data>> objects = dataFromSegments(frameName.toUri(), segments);
				
				if (isKey) publishedKey++;
				else publishedDelta++;

				for (auto& d:objects)
				{
					// std::cout << "add " << d->getName() << std::endl;
					cache.addData(d);
				}

				LogDebug("") << "published " << frameName << " " << published << std::endl;
			}
			return 0;
		};
	EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
		.WillRepeatedly(Invoke(publishFrame));

	int nInvalidated = 0;
	boost::function<void(PacketNumber, bool)> 
		requestFrame = [threadPrefix, &queue, &cache, buffer, &nInvalidated]
			(PacketNumber fno, bool key)
		{
			int nSeg = (key ? 10: 10);
			Name frameName(threadPrefix);
			
			if (key) frameName.append(NameComponents::NameComponentKey);
			else frameName.append(NameComponents::NameComponentDelta);
			
			frameName.appendSequenceNumber(fno);

			std::vector<boost::shared_ptr<Interest>> interests = getInterests(frameName.toUri(), 0, nSeg);
			ASSERT_TRUE(buffer->requested(makeInterestsConst(interests)));
			
			for (auto& i:interests)
			{
				queue.push([&queue, &cache, i, buffer, &nInvalidated](){
					cache.addInterest(i, [&queue, buffer, i, &nInvalidated](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest> i){
						queue.push([buffer, d, i, &nInvalidated](){
							// check whether buffer still has reserved slot - it may have been invalidated
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> s = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);
							if (buffer->isRequested(s))
								buffer->received(s);
							else
								nInvalidated++;
						});
					});
				});
			}
		};

	bool done = false;
	boost::function<void(void)> 
		requestNext = [&done, &request, &requestKey, &requestDelta, &requestFrame, cp, targetSize, samplePeriod, pqueue, &playout]
			()
		{
			if (!done && pqueue->size() >= targetSize && !playout.isRunning())
				EXPECT_NO_THROW(playout.start());
			
			if (playout.isRunning()) 
				EXPECT_LE(targetSize-1.5*samplePeriod, pqueue->size());

			if (request%cp.gop_ == 0)
				requestFrame(requestKey++, true);
			else
				requestFrame(requestDelta++, false);
			request++;
		};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.WillRepeatedly(Invoke(requestNext));

	int nPlayed = 0;
	VideoFrameSlot vs;
	playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed](const boost::shared_ptr<const BufferSlot>& slot)
	{
		bool recovered = false;
		boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
		EXPECT_TRUE(framePacket.get());
		nPlayed++;
	};

	// boost::function<void(void)> 
	// 	queueEmpty = [pipeline, &playout, &source, &pqueue, &buffer, streamPrefix, &nPlayed, &request, &published]
	// 		()
	// 	{
	// 		playout.stop();
	// 		LogTrace("") << buffer->dump() << std::endl;
	// 		EXPECT_FALSE(source.isRunning());
	// 		EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
	// 		EXPECT_EQ(pipeline, request-nPlayed);
	// 		EXPECT_EQ(nPlayed, published);
	// 		EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
	// 			BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	// 		EXPECT_EQ(request-nPlayed, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::New));
	// 		EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	// 		EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	// 		ASSERT_FALSE(playout.isRunning());
	// 	};

	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(1));
		// .WillOnce(Invoke(queueEmpty));

	// request initial frames
	for (request = 0; request < pipeline; ++request)
	{
		requestFrame((request%cp.gop_ ? requestDelta : requestKey), request%cp.gop_ == 0);
		if (request%cp.gop_ == 0) requestKey++;
		else requestDelta++;
	}

	source.start(captureFps);
	boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));
	done = true;
	source.stop();
	boost::this_thread::sleep_for(boost::chrono::milliseconds(2*oneWayDelay+4*deviation));
	playout.stop();

	EXPECT_FALSE(source.isRunning());
	EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
	EXPECT_EQ(pipeline, request-nPlayed);
	EXPECT_EQ(nPlayed+nInvalidated, published);
	EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
		BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	EXPECT_EQ(request-nPlayed-nInvalidated, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::New));
	EXPECT_GE(1, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	ASSERT_FALSE(playout.isRunning());

	work.reset();
	t.join();
}

TEST(TestPlayout, TestPlayout70msDelay)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#else
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int runTimeMs = 10000;
	int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
	int nPublished(0), nPublishedKey(0), nPublishedDelta(0); 
	int oneWayDelay = 70; // milliseconds
	int deviation = 0;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 22;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)oneWayDelay/(double)samplePeriod)+1;
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
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
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

			if (data->getSampleNo()+pp == seqNo)
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
	VideoFrameSlot vs;
	boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
	playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed, &buffer,  &lastTimestamp, samplePeriod,
										threadPrefix, &startPlayback]
		(const boost::shared_ptr<const BufferSlot>& slot)
	{
		boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
		lastTimestamp = p;

		if (startPlayback == boost::chrono::high_resolution_clock::time_point())
			startPlayback = p;

		bool recovered = false;
		boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
		EXPECT_TRUE(framePacket.get());
		nPlayed++;
	};

	int emptyCount = 0;
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&emptyCount]() {
			emptyCount++; 
		}));

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

	// allow some time to drain playout queue
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

	EXPECT_GT(pipeline*samplePeriod, abs(playbackDuration-publishDuration));
	EXPECT_FALSE(source.isRunning());
	EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
	EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
		BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	EXPECT_GE(1, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	
	EXPECT_GT(1, abs(avgPlayPeriod-avgPublishPeriod)); 	// ~1ms error
	EXPECT_GT(0.1, abs(avgPlayRate-avgPublishRate));	// ~0.1 error
	
	ASSERT_FALSE(playout.isRunning());

	ndnlog::new_api::Logger::getLoggerPtr("")->flush();
}

TEST(TestPlayout, TestPlayout100msDelay)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#else
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int runTimeMs = 10000;
	int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
	int nPublished(0), nPublishedKey(0), nPublishedDelta(0); 
	int oneWayDelay = 100; // milliseconds
	int deviation = 0;
	int targetSize = oneWayDelay+deviation;
	double captureFps = 22;
	int samplePeriod = (int)(1000./captureFps);
	int pipeline = 2*round((double)oneWayDelay/(double)samplePeriod)+1;
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
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
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

			if (data->getSampleNo()+pp == seqNo)
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
			// if (done && playout.isRunning())
			// 	playout.stop();

			pqueueSizeCnt++;
			pqueueSize += pqueue->size();

			if (pqueue->size() >= targetSize && !playout.isRunning() && !done)
				EXPECT_NO_THROW(playout.start());
		};

	EXPECT_CALL(pobserver, onNewSampleReady())
		.WillRepeatedly(Invoke(sampleReady));

	int nPlayed = 0;
	VideoFrameSlot vs;
	boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
	playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed, &buffer,  &lastTimestamp, samplePeriod,
										threadPrefix, &startPlayback]
		(const boost::shared_ptr<const BufferSlot>& slot)
	{
		boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
		lastTimestamp = p;

		if (startPlayback == boost::chrono::high_resolution_clock::time_point())
			startPlayback = p;

		bool recovered = false;
		boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
		EXPECT_TRUE(framePacket.get());
		nPlayed++;
	};

	int emptyCount = 0;
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&emptyCount]() {
			emptyCount++; 
		}));

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

	EXPECT_GT(pipeline*samplePeriod, abs(playbackDuration-publishDuration));
	EXPECT_FALSE(source.isRunning());
	EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
	EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
		BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	EXPECT_GE(1, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	
	EXPECT_GT(1, abs(avgPlayPeriod-avgPublishPeriod)); 	// ~1ms error
	EXPECT_GT(0.1, abs(avgPlayRate-avgPublishRate));	// ~0.1 error
	
	ASSERT_FALSE(playout.isRunning());

	ndnlog::new_api::Logger::getLoggerPtr("")->flush();
}

TEST(TestPlayout, TestPlayout100msDelay30msDeviation)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#else
	ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int runTimeMs = 10000;
	int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
	int nPublished(0), nPublishedKey(0), nPublishedDelta(0); 
	int oneWayDelay = 100; // milliseconds
	int deviation = 50;
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
	MockPlayoutObserver playoutObserver;
	PlayoutTest playout(io, pqueue);

#ifdef ENABLE_LOGGING
	playout.setDescription("playout");
	playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

	// buffer->attach(&bobserver);
	pqueue->attach(&pobserver);
	playout.attach(&playoutObserver);

#ifdef ENABLE_LOGGING
	buffer->setDescription("buffer");
	buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
	pqueue->setDescription("pqueue");
	pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
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
	VideoFrameSlot vs;
	boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
	playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed, &buffer,  &lastTimestamp, samplePeriod,
										threadPrefix, &startPlayback]
		(const boost::shared_ptr<const BufferSlot>& slot)
	{
		boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
		lastTimestamp = p;

		if (startPlayback == boost::chrono::high_resolution_clock::time_point())
			startPlayback = p;

		bool recovered = false;
		boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
		EXPECT_TRUE(framePacket.get());
		nPlayed++;
	};

	int emptyCount = 0;
	EXPECT_CALL(playoutObserver, onQueueEmpty())
		.Times(AtLeast(0))
		.WillRepeatedly(Invoke([&emptyCount]() {
			emptyCount++; 
		}));

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

	EXPECT_GT(pipeline*samplePeriod, abs(playbackDuration-publishDuration));
	EXPECT_FALSE(source.isRunning());
	EXPECT_LE(0, pqueue->size()); EXPECT_GT(50, pqueue->size());
	EXPECT_LT(0, buffer->getSlotsNum(Name(streamPrefix), 
		BufferSlot::New|BufferSlot::Assembling|BufferSlot::Ready));
	EXPECT_GE(1, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Assembling));
	EXPECT_EQ(0, buffer->getSlotsNum(Name(streamPrefix), BufferSlot::Ready));
	
	EXPECT_GT(1, abs(avgPlayPeriod-avgPublishPeriod)); 	// ~1ms error
	EXPECT_GT(0.1, abs(avgPlayRate-avgPublishRate));	// ~0.1 error
	
	ASSERT_FALSE(playout.isRunning());

	ndnlog::new_api::Logger::getLoggerPtr("")->flush();
}

TEST(TestPlayout, TestPlayoutFastForward)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#else
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelNone);
#endif
    
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });
    
    int runTimeMs = 10000;
    int nRequested(0), nRequestedKey(0), nRequestedDelta(0);
    int nPublished(0), nPublishedKey(0), nPublishedDelta(0);
    int oneWayDelay = 100; // milliseconds
    int deviation = 0;
    int targetSize = oneWayDelay+deviation;
    double captureFps = 22;
    int samplePeriod = (int)(1000./captureFps);
    int pipeline = 2*round((double)oneWayDelay/(double)samplePeriod)+1;
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
    MockPlayoutObserver playoutObserver;
    PlayoutTest playout(io, pqueue);
    
#ifdef ENABLE_LOGGING
    playout.setDescription("playout");
    playout.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
    
    // buffer->attach(&bobserver);
    pqueue->attach(&pobserver);
    playout.attach(&playoutObserver);
    
#ifdef ENABLE_LOGGING
    buffer->setDescription("buffer");
    buffer->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    pqueue->setDescription("pqueue");
    pqueue->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
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
        
        if (data->getSampleNo()+pp == seqNo)
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
        
        if (pqueue->size() >= 2*targetSize && !playout.isRunning() && !done)
            EXPECT_NO_THROW(playout.start(pqueue->size()-targetSize));
    };
    
    EXPECT_CALL(pobserver, onNewSampleReady())
    .WillRepeatedly(Invoke(sampleReady));
    
    int nPlayed = 0;
    VideoFrameSlot vs;
    boost::chrono::high_resolution_clock::time_point startPlayback, lastTimestamp;
    playout.pimpl()->onSampleReadyForPlayback = [&vs, &nPlayed, &buffer,  &lastTimestamp, samplePeriod,
                                        threadPrefix, &startPlayback]
    (const boost::shared_ptr<const BufferSlot>& slot)
    {
        boost::chrono::high_resolution_clock::time_point p = boost::chrono::high_resolution_clock::now();
        lastTimestamp = p;
        
        if (startPlayback == boost::chrono::high_resolution_clock::time_point())
            startPlayback = p;
        
        bool recovered = false;
        boost::shared_ptr<ImmutableVideoFramePacket> framePacket = vs.readPacket(*slot, recovered);
        EXPECT_TRUE(framePacket.get());
        nPlayed++;
    };
    
    int emptyCount = 0;
    EXPECT_CALL(playoutObserver, onQueueEmpty())
    .Times(AtLeast(0))
    .WillRepeatedly(Invoke([&emptyCount]() {
        emptyCount++;
    }));
    
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
    
    // allow some time to drain playout queue
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
    EXPECT_LT(80, avgQueueSize);
    EXPECT_LT(avgQueueSize, 120);
    
    ASSERT_FALSE(playout.isRunning());
    
    ndnlog::new_api::Logger::getLoggerPtr("")->flush();
}
#endif
//******************************************************************************
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
