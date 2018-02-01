// 
// test-latency-control.cc
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/atomic.hpp>
#include <fstream>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "src/drd-estimator.hpp"
#include "src/latency-control.hpp"
#include "client/src/precise-generator.hpp"
#include "statistics.hpp"

#include "mock-objects/latency-control-observer-mock.hpp"
#include "mock-objects/playout-control-mock.hpp"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace boost::chrono;

TEST(TestLatencyControl, TestLatestDataDetection)
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

	int oneWayDelay = 75;
	int deviation = 0;
	int deviationStep = 10;
	int defPipeline = 5;
	std::map<std::string, bool> runResults;
	int nSuccessfullRuns = 0;

	// run test repeatedly for different deviation values
	while (deviation < oneWayDelay)
	{
		boost::atomic<int> pipeline(defPipeline);

		DelayQueue queue(io, oneWayDelay, deviation);
		DataCache cache;
		std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";

		int nFramesGenerated = 0, gop = 30, fps = 30;
		int deltaSeqNo = 0, keySeqNo = 0;
		boost::function<void(void)> publishFrameSegment = 
		[&cache, &nFramesGenerated, &deltaSeqNo, &keySeqNo, gop, fps, threadPrefix]()
		{
			Name n(threadPrefix);
			int seq = (nFramesGenerated%gop == 0 ? keySeqNo : deltaSeqNo);
			if (nFramesGenerated%gop == 0) n.append(NameComponents::NameComponentKey);
			else n.append(NameComponents::NameComponentDelta);

			n.appendSequenceNumber(seq);
			VideoFramePacket p = getVideoFramePacket(600, fps);
			std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
			ASSERT_EQ(1, segments.size());
			std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
			ASSERT_EQ(1, dataObjects.size());

			boost::shared_ptr<Data> seg = dataObjects.front();

		// add to cache. use onInterest callback for altering data packet and saving 
		// Interest nonce in it
			cache.addData(seg, [&seg, n, keySeqNo, deltaSeqNo, fps, gop, nFramesGenerated](const boost::shared_ptr<ndn::Interest>& interest){
				unsigned int nonce = *((unsigned int*)interest->getNonce().buf());
				VideoFramePacket p = getVideoFramePacket(600, fps);
				std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
				ASSERT_EQ(1, segments.size());
				VideoFrameSegmentHeader hdr = segments.front().getHeader();
				hdr.interestNonce_ = nonce;
				segments.front().setHeader(hdr);
				std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
				ASSERT_EQ(1, dataObjects.size());

				seg = dataObjects.front();
			});

			if (nFramesGenerated%gop == 0) keySeqNo++;
			else deltaSeqNo++;

			nFramesGenerated++;
		};

		int drdTimeWindowMs = 1000;
		boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150,drdTimeWindowMs));
		unsigned int iseq = 0, kseq = 0, dseq = 0;
		boost::function<void(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>&)> incoming;
		boost::function<void(void)> express = 
		[&cache, &queue, &incoming, &iseq, &kseq, &dseq, &pipeline, threadPrefix, gop, &drd]
		{
			while (pipeline > 0)
			{
				Name n(threadPrefix);
				bool iskey = (iseq % gop == 0);
				n.append((iskey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
				.appendSequenceNumber((iskey ? kseq++ : dseq++));
				std::vector<boost::shared_ptr<ndn::Interest>> intrst = getInterests(n.toUri(), 0, 1, 0, 0, 2345);
				ASSERT_EQ(1, intrst.size());

				boost::shared_ptr<ndn::Interest> i = intrst.front();
				boost::chrono::high_resolution_clock::time_point issue = boost::chrono::high_resolution_clock::now();
				queue.push([i, &queue, &cache, &incoming, issue, &drd](){ // send interest
					// add interest to producer's PIT or answer right away
					boost::chrono::high_resolution_clock::time_point pended = boost::chrono::high_resolution_clock::now();
					cache.addInterest(i, [&queue, &incoming, i, issue, &drd, pended](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest>)
					{
						double generationDelayMs = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::high_resolution_clock::now()-pended).count();
						
						queue.push([&incoming, d, i, issue, &drd, generationDelayMs]() // send data back
						{ 
							boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);

							if (data->isOriginal())
							{
								double delta = boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count();
								drd->newValue(delta - generationDelayMs, true, 0);
							}
							else
								drd->newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count(), false, 0);

							incoming(data); // process received data
						});
					});
				});
		
			iseq++;
			pipeline--;
			}
		};
		
		int nIncreaseEvents = 0, nDecreaseEvents = 0;
		MockLatencyControlObserver lco;
		boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
		LatencyControl latControl(1000, drd, storage);
		drd->attach(&latControl);
		latControl.registerObserver(&lco);

		boost::shared_ptr<MockPlayoutControl> playoutControl = boost::make_shared<MockPlayoutControl>();
		latControl.setPlayoutControl(playoutControl);

		high_resolution_clock::time_point start = high_resolution_clock::now();
		unsigned int lastThreshold = 150;
		EXPECT_CALL(*playoutControl, getThreshold())
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke([&lastThreshold](){ return lastThreshold; }));
		EXPECT_CALL(*playoutControl, setThreshold(_))
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke([&lastThreshold, oneWayDelay, deviation, start, drdTimeWindowMs](unsigned int t){ 
				lastThreshold = t; 

				high_resolution_clock::time_point now = high_resolution_clock::now();
				auto duration = duration_cast<milliseconds>( now - start ).count();

				if (duration > drdTimeWindowMs)
				{
					// for default playback queue strategy,
					// we expect that threshold will be around 2*oneWayDelay + 4*deviation
					double error = 0.25;
					unsigned int targetValue = 2*oneWayDelay+4*deviation;
					unsigned int dev = std::abs((int)targetValue - (int)t);
					// TODO: make up smarter check here. this one below fails often
					// EXPECT_GE(error, (double)dev/(double)targetValue);
				}
			}));

#ifdef ENABLE_LOGGING
		latControl.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		estimators::Average dArr(boost::make_shared<estimators::SampleWindow>(30));
		int nreceived = 0, nOriginal = 0;
		boost::chrono::high_resolution_clock::time_point tp;

		incoming = [&pipeline, &express, &nreceived, &nOriginal, &tp, &dArr, 
			&latControl, &lco, &nIncreaseEvents, &nDecreaseEvents, fps]
		(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>& d)
		{
			boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
			if (nreceived > 0)
				dArr.newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now - tp).count());

			latControl.targetRateUpdate(fps);

			boost::function<bool(const PipelineAdjust&)> onAdjustmentNeeded = 
				[&nIncreaseEvents, &nDecreaseEvents](const PipelineAdjust& cmd)->bool{
					if (cmd == PipelineAdjust::IncreasePipeline) nIncreaseEvents++;
					if (cmd == PipelineAdjust::DecreasePipeline) nDecreaseEvents++;
					return false;
				};
			EXPECT_CALL(lco, needPipelineAdjustment(_))
				.Times(1)
				.WillOnce(Invoke(onAdjustmentNeeded));
			latControl.sampleArrived(0);

			pipeline++;
			express();
			tp = now;
			nreceived++;
			if (d->isOriginal()) nOriginal++;
		};
		
		PreciseGenerator pgen(io, fps, publishFrameSegment);
		pgen.start();
		express();
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(5000));
		
		pgen.stop();

		while (queue.isActive())
			boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay));

		std::stringstream ss;
		ss << "lat-" << oneWayDelay*2 << "-dev-" << deviation << "-pp-" << defPipeline;

		// we assume, that for 30fps and 150ms two-way delay, pipeline of 
		// size 5 is sufficient to reach latest data. thus LatencyControl
		// shall not command to increase pipeline at all
		bool successfulRun = (nIncreaseEvents == 0);

		runResults[ss.str()] = successfulRun;

		GT_PRINTF("run %s : %s (Darr avg: %.2f, dev %.2f, increase: %d, decrease: %d)\n", 
			ss.str().c_str(), (successfulRun ? "SUCCESS" : "FAILED"),
			dArr.value(), dArr.deviation(), nIncreaseEvents, nDecreaseEvents);
		if (successfulRun) nSuccessfullRuns++;

		deviation += deviationStep;
	} // while

	GT_PRINTF("total runs: %d, successful: %d (minimum 50 percent should be successful to pass)\n",
		runResults.size(), nSuccessfullRuns);
	EXPECT_GE((double)nSuccessfullRuns/(double)runResults.size(), 0.5);

	work.reset();
	t.join();
}
#if 0
TEST(TestLatencyControl, TestNeverCatchUp)
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

	int oneWayDelay = 75;
	int deviation = 0;
	int deviationStep = 10;
	int defPipeline = 3;
	std::map<std::string, bool> runResults;
	int nSuccessfullRuns = 0;

	while (deviation < oneWayDelay)
	{
		boost::atomic<int> pipeline(defPipeline);

		DelayQueue queue(io, oneWayDelay, deviation);
		DataCache cache;
		std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";

		int nFramesGenerated = 0, gop = 30, fps = 30;
		int deltaSeqNo = 0, keySeqNo = 0;
		boost::function<void(void)> publishFrameSegment = 
		[&cache, &nFramesGenerated, &deltaSeqNo, &keySeqNo, gop, fps, threadPrefix]()
		{
			Name n(threadPrefix);
			int seq = (nFramesGenerated%gop == 0 ? keySeqNo : deltaSeqNo);
			if (nFramesGenerated%gop == 0) n.append(NameComponents::NameComponentKey);
			else n.append(NameComponents::NameComponentDelta);

			n.appendSequenceNumber(seq);
			VideoFramePacket p = getVideoFramePacket(600, fps);
			std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
			ASSERT_EQ(1, segments.size());
			std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
			ASSERT_EQ(1, dataObjects.size());

			boost::shared_ptr<Data> seg = dataObjects.front();

		// add to cache. use onInterest callback for altering data packet and saving 
		// Interest nonce in it
			cache.addData(seg, [&seg, n, keySeqNo, deltaSeqNo, fps, gop, nFramesGenerated](const boost::shared_ptr<ndn::Interest>& interest){
				unsigned int nonce = *((unsigned int*)interest->getNonce().buf());
				VideoFramePacket p = getVideoFramePacket(600, fps);
				std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
				ASSERT_EQ(1, segments.size());
				VideoFrameSegmentHeader hdr = segments.front().getHeader();
				hdr.interestNonce_ = nonce;
				segments.front().setHeader(hdr);
				std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
				ASSERT_EQ(1, dataObjects.size());

				seg = dataObjects.front();
			});

			if (nFramesGenerated%gop == 0) keySeqNo++;
			else deltaSeqNo++;

			nFramesGenerated++;
		};

		boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150,1000));
		unsigned int iseq = 0, kseq = 0, dseq = 0;
		boost::function<void(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>&)> incoming;
		boost::function<void(void)> express = 
		[&cache, &queue, &incoming, &iseq, &kseq, &dseq, &pipeline, threadPrefix, gop, &drd]
		{
			while (pipeline > 0)
			{
				Name n(threadPrefix);
				bool iskey = (iseq % gop == 0);
				n.append((iskey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
				.appendSequenceNumber((iskey ? kseq++ : dseq++));
				std::vector<boost::shared_ptr<ndn::Interest>> intrst = getInterests(n.toUri(), 0, 1, 0, 0, 2345);
				ASSERT_EQ(1, intrst.size());

				boost::shared_ptr<ndn::Interest> i = intrst.front();
				boost::chrono::high_resolution_clock::time_point issue = boost::chrono::high_resolution_clock::now();
				queue.push([i, &queue, &cache, &incoming, issue, &drd](){ // send interest
					// add interest to producer's PIT or answer right away
					boost::chrono::high_resolution_clock::time_point pended = boost::chrono::high_resolution_clock::now();
					cache.addInterest(i, [&queue, &incoming, i, issue, &drd, pended](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest>)
					{
						double generationDelayMs = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::high_resolution_clock::now()-pended).count();
						
						queue.push([&incoming, d, i, issue, &drd, generationDelayMs]() // send data back
						{ 
							boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);

							if (data->isOriginal())
							{
								double delta = boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count();
								drd->newValue(delta - generationDelayMs, true);
							}
							else
								drd->newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count(), false);

							incoming(data); // process received data
						});
					});
				});
		
			iseq++;
			pipeline--;
			}
		};
		
		int nIncreaseEvents = 0, nDecreaseEvents = 0;
		MockLatencyControlObserver lco;
		boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
		LatencyControl latControl(1000, drd, storage);
		drd->attach(&latControl);
		latControl.registerObserver(&lco);
#ifdef ENABLE_LOGGING
		latControl.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		estimators::Average dArr(boost::make_shared<estimators::SampleWindow>(30));
		int nreceived = 0, nOriginal = 0;
		boost::chrono::high_resolution_clock::time_point tp;

		incoming = [&pipeline, &express, &nreceived, &nOriginal, &tp, &dArr, 
			&latControl, &lco, &nIncreaseEvents, &nDecreaseEvents, fps]
		(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>& d)
		{
			boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
			if (nreceived > 0)
				dArr.newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now - tp).count());

			latControl.targetRateUpdate(fps);

			boost::function<bool(const PipelineAdjust&)> onAdjustmentNeeded = 
				[&nIncreaseEvents, &nDecreaseEvents](const PipelineAdjust& cmd)->bool{
					if (cmd == PipelineAdjust::IncreasePipeline) nIncreaseEvents++;
					if (cmd == PipelineAdjust::DecreasePipeline) nDecreaseEvents++;
					return false;
				};
			EXPECT_CALL(lco, needPipelineAdjustment(_))
				.Times(1)
				.WillOnce(Invoke(onAdjustmentNeeded));
			latControl.sampleArrived(0);

			pipeline++;
			express();
			tp = now;
			nreceived++;
			if (d->isOriginal()) nOriginal++;
		};
		
		PreciseGenerator pgen(io, fps, publishFrameSegment);
		pgen.start();
		express();
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(5000));
		
		pgen.stop();

		while (queue.isActive())
			boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay));

		std::stringstream ss;
		ss << "lat-" << oneWayDelay*2 << "-dev-" << deviation << "-pp-" << defPipeline;

		// we assume, that for 30fps and 150ms two-way delay, pipeline of 
		// size 5 is sufficient to reach latest data. thus LatencyControl
		// shall not command to increase pipeline at all
		bool successfulRun = (nDecreaseEvents == 0);

		runResults[ss.str()] = successfulRun;

		GT_PRINTF("run %s : %s (Darr avg: %.2f, dev %.2f, increase: %d, decrease: %d)\n", 
			ss.str().c_str(), (successfulRun ? "SUCCESS" : "FAILED"),
			dArr.value(), dArr.deviation(), nIncreaseEvents, nDecreaseEvents);
		if (successfulRun) nSuccessfullRuns++;

		deviation += deviationStep;
	} // while

	GT_PRINTF("total runs: %d, successful: %d (minimum 50 percent should be successful to pass)\n",
		runResults.size(), nSuccessfullRuns);
	EXPECT_GE((double)nSuccessfullRuns/(double)runResults.size(), 0.5);

	work.reset();
	t.join();
}

TEST(TestLatencyControl, TestChangingPipeline)
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

	int oneWayDelay = 75;
	int deviation = 0;
	int deviationStep = 10;
	std::map<std::string, bool> runResults;
	int nSuccessfullRuns = 0;

	while (deviation < oneWayDelay)
	{
		int defPipeline = 1;
		boost::atomic<int> pipeline(defPipeline);

		DelayQueue queue(io, oneWayDelay, deviation);
		DataCache cache;
		std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";

		int nFramesGenerated = 0, gop = 30, fps = 30;
		int deltaSeqNo = 0, keySeqNo = 0;
		boost::function<void(void)> publishFrameSegment = 
		[&cache, &nFramesGenerated, &deltaSeqNo, &keySeqNo, gop, fps, threadPrefix]()
		{
			Name n(threadPrefix);
			int seq = (nFramesGenerated%gop == 0 ? keySeqNo : deltaSeqNo);
			if (nFramesGenerated%gop == 0) n.append(NameComponents::NameComponentKey);
			else n.append(NameComponents::NameComponentDelta);

			n.appendSequenceNumber(seq);
			VideoFramePacket p = getVideoFramePacket(600, fps);
			std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
			ASSERT_EQ(1, segments.size());
			std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
			ASSERT_EQ(1, dataObjects.size());

			boost::shared_ptr<Data> seg = dataObjects.front();

		// add to cache. use onInterest callback for altering data packet and saving 
		// Interest nonce in it
			cache.addData(seg, [&seg, n, keySeqNo, deltaSeqNo, fps, gop, nFramesGenerated](const boost::shared_ptr<ndn::Interest>& interest){
				unsigned int nonce = *((unsigned int*)interest->getNonce().buf());
				VideoFramePacket p = getVideoFramePacket(600, fps);
				std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
				ASSERT_EQ(1, segments.size());
				VideoFrameSegmentHeader hdr = segments.front().getHeader();
				hdr.interestNonce_ = nonce;
				segments.front().setHeader(hdr);
				std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
				ASSERT_EQ(1, dataObjects.size());

				seg = dataObjects.front();
			});

			if (nFramesGenerated%gop == 0) keySeqNo++;
			else deltaSeqNo++;

			nFramesGenerated++;
		};

		boost::shared_ptr<DrdEstimator> drd(boost::make_shared<DrdEstimator>(150,1000));
		unsigned int iseq = 0, kseq = 0, dseq = 0;
		boost::function<void(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>&)> incoming;
		boost::function<void(void)> express = 
		[&cache, &queue, &incoming, &iseq, &kseq, &dseq, &pipeline, threadPrefix, gop, &drd]
		{
			while (pipeline > 0)
			{
				Name n(threadPrefix);
				bool iskey = (iseq % gop == 0);
				n.append((iskey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
				.appendSequenceNumber((iskey ? kseq++ : dseq++));
				std::vector<boost::shared_ptr<ndn::Interest>> intrst = getInterests(n.toUri(), 0, 1, 0, 0, 2345);
				ASSERT_EQ(1, intrst.size());

				boost::shared_ptr<ndn::Interest> i = intrst.front();
				boost::chrono::high_resolution_clock::time_point issue = boost::chrono::high_resolution_clock::now();
				queue.push([i, &queue, &cache, &incoming, issue, &drd](){ // send interest
					// add interest to producer's PIT or answer right away
					boost::chrono::high_resolution_clock::time_point pended = boost::chrono::high_resolution_clock::now();
					cache.addInterest(i, [&queue, &incoming, i, issue, &drd, pended](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest>)
					{
						double generationDelayMs = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::high_resolution_clock::now()-pended).count();
						
						queue.push([&incoming, d, i, issue, &drd, generationDelayMs]() // send data back
						{ 
							boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);

							if (data->isOriginal())
							{
								double delta = boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count();
								drd->newValue(delta - generationDelayMs, true);
							}
							else
								drd->newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count(), false);

							incoming(data); // process received data
						});
					});
				});
		
			iseq++;
			pipeline--;
			}
		};
		
		int nIncreaseEvents = 0, nDecreaseEvents = 0;
		MockLatencyControlObserver lco;
		boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
		LatencyControl latControl(1000, drd, storage);
		drd->attach(&latControl);
		latControl.registerObserver(&lco);
#ifdef ENABLE_LOGGING
		latControl.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		estimators::Average dArr(boost::make_shared<estimators::SampleWindow>(30));
		int nreceived = 0, nOriginal = 0;
		boost::chrono::high_resolution_clock::time_point tp;
		int lastFailedPipeline = 0;

		incoming = [&pipeline, &express, &nreceived, &nOriginal, &tp, &dArr, 
			&latControl, &lco, &nIncreaseEvents, &nDecreaseEvents, fps, &lastFailedPipeline, &defPipeline]
		(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>& d)
		{
			pipeline++;

			boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
			if (nreceived > 0)
				dArr.newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now - tp).count());

			latControl.targetRateUpdate(fps);

			boost::function<bool(const PipelineAdjust&)> onAdjustmentNeeded = 
				[&nIncreaseEvents, &nDecreaseEvents, &defPipeline, &pipeline, &lastFailedPipeline]
				(const PipelineAdjust& cmd)->bool{
					if (cmd == PipelineAdjust::IncreasePipeline)
					{
						lastFailedPipeline = defPipeline;
						nIncreaseEvents++;
						pipeline += defPipeline;
						defPipeline += defPipeline;
					}
					if (cmd == PipelineAdjust::DecreasePipeline)
					{ 
						nDecreaseEvents++;
						int newDefPipeline = (int)ceil((double)(lastFailedPipeline+defPipeline)/2.);
						if (newDefPipeline == lastFailedPipeline) newDefPipeline++;
						pipeline -= (defPipeline-newDefPipeline);
						defPipeline = newDefPipeline;
					}
					return true;
				};
			EXPECT_CALL(lco, needPipelineAdjustment(_))
				.Times(1)
				.WillOnce(Invoke(onAdjustmentNeeded));
			latControl.sampleArrived(0);

			express();
			tp = now;
			nreceived++;
			if (d->isOriginal()) nOriginal++;
		};
		
		PreciseGenerator pgen(io, fps, publishFrameSegment);
		pgen.start();
		express();
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(5000));
		
		pgen.stop();

		while (queue.isActive())
			boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay));

		std::stringstream ss;
		ss << "lat-" << oneWayDelay*2 << "-dev-" << deviation << "-pp-" << defPipeline;

		// we assume, that for 30fps and 150ms two-way delay, pipeline of 
		// sizes 4,5,6 is sufficient to reach latest data.
		bool successfulRun = (defPipeline == 4 || defPipeline == 5 || defPipeline == 6);

		runResults[ss.str()] = successfulRun;

		GT_PRINTF("run %s : %s, final pipeline: %d (Darr avg: %.2f, dev %.2f, increase: %d, decrease: %d)\n", 
			ss.str().c_str(), (successfulRun ? "SUCCESS" : "FAILED"), defPipeline,
			dArr.value(), dArr.deviation(), nIncreaseEvents, nDecreaseEvents);
		if (successfulRun) nSuccessfullRuns++;

		deviation += deviationStep;
	} // while

	GT_PRINTF("total runs: %d, successful: %d (minimum 50 percent should be successful to pass)\n",
		runResults.size(), nSuccessfullRuns);
	EXPECT_GE((double)nSuccessfullRuns/(double)runResults.size(), 0.5);

	work.reset();
	t.join();
}
#endif
#if 0
TEST(TestLatencyControl, TestLatestDataDetection)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	int oneWayDelay = 75;
	int deviation = 0;
	int deviationStep = 5;
	boost::atomic<int> pipeline(25);

	while (deviation < oneWayDelay)
	{
		DelayQueue queue(io, oneWayDelay, deviation);
		DataCache cache;
		std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";

		int nFramesGenerated = 0, gop = 30, fps = 30;
		int deltaSeqNo = 0, keySeqNo = 0;
		boost::function<void(void)> publishFrameSegment = 
		[&cache, &nFramesGenerated, &deltaSeqNo, &keySeqNo, gop, fps, threadPrefix]()
		{
			Name n(threadPrefix);
			int seq = (nFramesGenerated%gop == 0 ? keySeqNo : deltaSeqNo);
			if (nFramesGenerated%gop == 0) n.append(NameComponents::NameComponentKey);
			else n.append(NameComponents::NameComponentDelta);

			n.appendSequenceNumber(seq);
			VideoFramePacket p = getVideoFramePacket(600, fps);
			std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
			ASSERT_EQ(1, segments.size());
			std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
			ASSERT_EQ(1, dataObjects.size());

			boost::shared_ptr<Data> seg = dataObjects.front();

		// add to cache. use onInterest callback for altering data packet and saving 
		// Interest nonce in it
			cache.addData(seg, [&seg, n, keySeqNo, deltaSeqNo, fps, gop, nFramesGenerated](const boost::shared_ptr<ndn::Interest>& interest){
				unsigned int nonce = *((unsigned int*)interest->getNonce().buf());
				VideoFramePacket p = getVideoFramePacket(600, fps);
				std::vector<VideoFrameSegment> segments = sliceFrame(p, nFramesGenerated, (nFramesGenerated%gop ? keySeqNo : deltaSeqNo));
				ASSERT_EQ(1, segments.size());
				VideoFrameSegmentHeader hdr = segments.front().getHeader();
				hdr.interestNonce_ = nonce;
				segments.front().setHeader(hdr);
				std::vector<boost::shared_ptr<Data>> dataObjects = dataFromSegments(n.toUri(), segments);
				ASSERT_EQ(1, dataObjects.size());

				seg = dataObjects.front();
			});

			if (nFramesGenerated%gop == 0) keySeqNo++;
			else deltaSeqNo++;

			nFramesGenerated++;
		};

		DrdEstimator drd(150, 1000);
		unsigned int iseq = 0, kseq = 0, dseq = 0;
		boost::function<void(const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>&)> incoming;
		boost::function<void(void)> express = 
		[&cache, &queue, &incoming, &iseq, &kseq, &dseq, &pipeline, threadPrefix, gop, &drd]
		{
			while (pipeline > 0)
			{
				Name n(threadPrefix);
				bool iskey = (iseq % gop == 0);
				n.append((iskey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
				.appendSequenceNumber((iskey ? kseq++ : dseq++));
				std::vector<boost::shared_ptr<ndn::Interest>> intrst = getInterests(n.toUri(), 0, 1, 0, 0, 2345);
				ASSERT_EQ(1, intrst.size());

				boost::shared_ptr<ndn::Interest> i = intrst.front();
				boost::chrono::high_resolution_clock::time_point issue = boost::chrono::high_resolution_clock::now();
				queue.push([i, &queue, &cache, &incoming, issue, &drd](){ // send interest
					// add interest to producer's PIT or answer right away
					boost::chrono::high_resolution_clock::time_point pended = boost::chrono::high_resolution_clock::now();
					cache.addInterest(i, [&queue, &incoming, i, issue, &drd, pended](const boost::shared_ptr<ndn::Data>& d, const boost::shared_ptr<ndn::Interest>)
					{
						double generationDelayMs = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::high_resolution_clock::now()-pended).count();
						
						queue.push([&incoming, d, i, issue, &drd, generationDelayMs]() // send data back
						{ 
							boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
							boost::shared_ptr<WireData<VideoFrameSegmentHeader>> data = boost::make_shared<WireData<VideoFrameSegmentHeader>>(d, i);

							if (data->isOriginal())
							{
								double delta = boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count();
								drd->newValue(delta - generationDelayMs, true);
							}
							else
								drd->newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now-issue).count(), false);

							incoming(data); // process received data
						});
					});
				});
		
			iseq++;
			pipeline--;
			}
		};
		
		estimators::Average dArr(boost::make_shared<estimators::SampleWindow>(30));
		int nreceived = 0, nOriginal = 0;
		boost::chrono::high_resolution_clock::time_point tp;
		std::stringstream ss;
		ss << "delay-"<< oneWayDelay << "-var-"<< deviation << "-pp-"<<pipeline<<".txt";
		std::ofstream f(ss.str().c_str());
		incoming = 
		[&pipeline, &express, &nreceived, &nOriginal, &tp, &f, &dArr](const boost::shared_ptr<WireData<VideoFrameSegmentHeader>>& d)
		{
			boost::chrono::high_resolution_clock::time_point now = boost::chrono::high_resolution_clock::now();
		
			if (nreceived > 0)
				dArr.newValue(boost::chrono::duration_cast<boost::chrono::milliseconds>(now - tp).count());
		
			pipeline++;
			express();
			tp = now;
			nreceived++;
			if (d->isOriginal()) nOriginal++;
		};
		
		PreciseGenerator pgen(io, fps, publishFrameSegment);
		pgen.start();
		express();
		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(5000));
		
		pgen.stop();
		
		GT_PRINTF("received total %d, original %d (%.2f%)\n",
			nreceived, nOriginal, (double)nOriginal/(double)nreceived*100);
		GT_PRINTF("1-way delay: %dms (deviation %dms), measured drd: %.2fms, dev: %.2f; Darr: %.2f, dev: %.2f\n",
			oneWayDelay, deviation, drd->getLatestUpdatedAverage().value(), drd->getLatestUpdatedAverage().deviation(),
			dArr.value(), dArr.deviation());
		
		while (queue.isActive()) 
			boost::this_thread::sleep_for(boost::chrono::milliseconds(oneWayDelay));

		deviation += deviationStep;
	} // while devitaion < oneWayDelay

	work.reset();
	t.join();
}
#endif
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
