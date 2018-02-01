// 
// test-media-thread.cc
//
//  Created by Peter Gusev on 08 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread/future.hpp>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>
#include <stdlib.h>
#include <ndn-cpp/data.hpp>
#include "gtest/gtest.h"

#include "tests-helpers.hpp"
#include "frame-data.hpp"
#include "src/video-thread.hpp"
#include "src/audio-thread.hpp"
#include "mock-objects/audio-thread-callback-mock.hpp"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace boost::chrono;

#if 1
TEST(TestVideoThread, TestCreate)
{
	VideoThread vt(sampleVideoCoderParams());
}

TEST(TestVideoThread, TestEncodeSimple)
{
	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 700;
	int width = 1280, height = 720;
	WebRtcVideoFrame convertedFrame(std::move(getFrame(width, height)));

	{ // try 1 frame
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;

		VideoThread vt(vcp);
		boost::shared_ptr<VideoFramePacket> vf(boost::move(vt.encode(convertedFrame)));
		EXPECT_EQ(width, vf->getFrame()._encodedWidth);
		EXPECT_EQ(height, vf->getFrame()._encodedHeight);
		EXPECT_TRUE(vf->getFrame()._completeFrame);
		EXPECT_EQ(webrtc::kVideoFrameKey, vf->getFrame()._frameType);
	}
}

TEST(TestVideoThread, TestEncodeAsync)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	bool dropEnabled = true;
	int nFrames = 30;
	int targetBitrate = 700;
	int width = 1280, height = 720;
	WebRtcVideoFrame convertedFrame(std::move(getFrame(width, height)));

	{ // try 1 frame to encode asynchronously
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;

		VideoThread vt(vcp);
#ifdef ENABLE_LOGGING
		vt.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
		boost::future<boost::shared_ptr<VideoFramePacket>> futurePacket = 
			boost::async(boost::launch::async, boost::bind(&VideoThread::encode, &vt, convertedFrame));

#ifdef ENABLE_LOGGING
		boost::this_thread::sleep(boost::posix_time::seconds(1));
		LogInfo("") << "getting future frame..." << std::endl;
#endif

		boost::shared_ptr<VideoFramePacket> vf(futurePacket.get());

#ifdef ENABLE_LOGGING
		LogInfo("") << "got frame" << std::endl;
#endif

		EXPECT_TRUE(vf.get());
		EXPECT_LE(0, vf->getLength());
		EXPECT_EQ(width, vf->getFrame()._encodedWidth);
		EXPECT_EQ(height, vf->getFrame()._encodedHeight);
		EXPECT_TRUE(vf->getFrame()._completeFrame);
		EXPECT_EQ(webrtc::kVideoFrameKey, vf->getFrame()._frameType);
	}
}

TEST(TestVideoThread, TestEncodeMultipleThreads)
{
	bool dropEnabled = true;
	int nFrames = 90;
	int width = 1280, height = 720;
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	std::vector<WebRtcVideoFrame> frames = getFrameSequence(width, height, nFrames);

	std::vector<VideoCoderParams> coderParams;
	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 3000;
		vcp.maxBitrate_ = 3000;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;
		coderParams.push_back(vcp);
	}
	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width/2;
		vcp.encodeHeight_ = height/2;
		coderParams.push_back(vcp);
	}
	{
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 200;
		vcp.maxBitrate_ = 200;
		vcp.encodeWidth_ = width/4;
		vcp.encodeHeight_ = height/4;
		coderParams.push_back(vcp);
	}

	std::vector<boost::shared_ptr<VideoThread>> videoThreads;
	std::vector<boost::shared_ptr<FrameScaler>> scaleFrame;
	std::vector<std::pair<uint64_t, uint64_t>> seqCounters;
	for (auto params:coderParams) 
	{
		videoThreads.push_back(boost::make_shared<VideoThread>(params));
		scaleFrame.push_back(boost::make_shared<FrameScaler>(params.encodeWidth_, params.encodeHeight_));
		seqCounters.push_back(std::pair<uint64_t, uint64_t>(0,0));

#ifdef ENABLE_LOGGING
		videoThreads.back()->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
	}
	
	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	typedef boost::shared_ptr<VideoFramePacket> FramePacketPtr;
	typedef boost::future<FramePacketPtr> FutureFrame;
	typedef boost::shared_ptr<boost::future<FramePacketPtr>> FutureFramePtr;

	for (int i = 0; i < nFrames; ++i)
	{
		runTimer.expires_from_now(boost::posix_time::milliseconds(30));
		std::vector<FutureFramePtr> futureFrames;

		for (int k = 0; k < videoThreads.size(); ++k)
		{
			FutureFramePtr ff = boost::make_shared<FutureFrame>(boost::move(boost::async(boost::launch::async, 
					boost::bind(&VideoThread::encode, videoThreads[k].get(), (*scaleFrame[k])(frames[i])))));
			futureFrames.push_back(ff);
		}

		int idx = 0;
		for (auto ff:futureFrames)
		{
			boost::shared_ptr<VideoFramePacket> vf(ff->get());
			if (vf.get())
			{
				if (vf->getFrame()._frameType == webrtc::kVideoFrameKey)
					seqCounters[idx].first++;
				else
					seqCounters[idx].second++;
			}
			idx++;
		}
		
		runTimer.wait();
	}

	for (int i = 0; i < videoThreads.size(); ++i)
	{
		EXPECT_LE(0, videoThreads[i]->getEncodedNum());
		GT_PRINTF("thread %d (%dx%d %d Kbit/s): %d encoded, %d dropped, %d key, %d delta\n", 
			i, coderParams[i].encodeWidth_, coderParams[i].encodeHeight_, 
			coderParams[i].startBitrate_, 
			videoThreads[i]->getEncodedNum(), videoThreads[i]->getDroppedNum(),
			seqCounters[i].first, seqCounters[i].second);
	}
}
#endif

TEST(TestAudioThread, TestRunOpusThread)
{
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

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [&callbackTs, wire_length,
	 &bundle, &nBundles, &bundleNo](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		nBundles++;
		if (bundleNo) EXPECT_LT(bundleNo, n);
		bundleNo = n;
		EXPECT_LE(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
		callbackTs = high_resolution_clock::now();

		CommonHeader packetHdr;
		packetHdr.sampleRate_ = 25;
		packetHdr.publishTimestampMs_ = nBundles+1;
		packetHdr.publishUnixTimestamp_ = nBundles+2;
		bundle->setHeader(packetHdr);

		EXPECT_EQ(25, bundle->getHeader().sampleRate_);
		EXPECT_EQ(nBundles+1, bundle->getHeader().publishTimestampMs_);
		EXPECT_EQ(nBundles+2, bundle->getHeader().publishUnixTimestamp_);
		EXPECT_LE(1, bundle->getSamplesNum());
	};

	EXPECT_CALL(callback, onSampleBundle("hd",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));
	
	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(1000));

	EXPECT_NO_THROW(at.start());
	EXPECT_ANY_THROW(at.start());

	runTimer.wait();
	
	EXPECT_NO_THROW(at.stop());
	ASSERT_GT(high_resolution_clock::now(), callbackTs);
	EXPECT_LE(1, nBundles);
	EXPECT_GE(wire_length, bundle->getLength());
	ASSERT_LE(1, bundle->getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle->getSamplesNum(), bundle->getLength(), bundle->operator[](0).size());
}
#if 1
TEST(TestAudioThread, TestRunG722Thread)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("g722");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	boost::shared_ptr<AudioBundlePacket> bundle(boost::make_shared<AudioBundlePacket>(wire_length));
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;
	uint64_t bundleNo = 0;

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [wire_length, &bundle, &nBundles, &bundleNo](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		nBundles++;
		if (bundleNo) EXPECT_LT(bundleNo, n);
		bundleNo = n;
		EXPECT_LE(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
	};

	EXPECT_CALL(callback, onSampleBundle("g722", _, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));
	
	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(1000));

	EXPECT_NO_THROW(at.start());
	EXPECT_ANY_THROW(at.start());

	runTimer.wait();
	
	EXPECT_NO_THROW(at.stop());
	EXPECT_LE(1, nBundles);
	EXPECT_GE(wire_length, bundle->getLength());
	EXPECT_LE(1, bundle->getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle->getSamplesNum(), bundle->getLength(), bundle->operator[](0).size());
}

TEST(TestAudioThread, TestRun8kSegment)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("g722");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 8000;
	boost::shared_ptr<AudioBundlePacket> bundle(boost::make_shared<AudioBundlePacket>(wire_length));
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;
	high_resolution_clock::time_point callbackTs;

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [&callbackTs, wire_length,
	 &bundle, &nBundles](std::string, uint64_t, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		nBundles++;
		EXPECT_LT(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
		callbackTs = high_resolution_clock::now();
	};

	EXPECT_CALL(callback, onSampleBundle("g722",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));
	
	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(1000));

	EXPECT_NO_THROW(at.start());
	EXPECT_ANY_THROW(at.start());

	runTimer.wait();
	
	EXPECT_NO_THROW(at.stop());
	ASSERT_GT(high_resolution_clock::now(), callbackTs);
	EXPECT_LE(1, nBundles);
	EXPECT_GE(wire_length, bundle->getLength());
	EXPECT_LT(1, bundle->getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle->getSamplesNum(), bundle->getLength(), bundle->operator[](0).size());
}

TEST(TestAudioThread, TestRunMultipleThreads)
{
	MockAudioThreadCallback callback1, callback2;
	AudioThreadParams ap1("sd", "opus");
	AudioThreadParams ap2("hd");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	AudioBundlePacket bundle1(wire_length);
	AudioBundlePacket bundle2(wire_length);
	AudioThread at1(ap1, acp, &callback1, wire_length);
	AudioThread at2(ap2, acp, &callback2, wire_length);
	int nBundles1 = 0, nBundles2 = 0;

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle1 = [wire_length, &bundle1, &nBundles1](std::string, uint64_t, boost::shared_ptr<AudioBundlePacket> b){
		bundle1.swap(*b);
		nBundles1++;
		EXPECT_LT(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
	};
	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle2 = [wire_length, &bundle2, &nBundles2](std::string, uint64_t, boost::shared_ptr<AudioBundlePacket> b){
		bundle2.swap(*b);
		nBundles2++;
		EXPECT_LT(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
	};

	EXPECT_CALL(callback1, onSampleBundle("sd",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle1));
	EXPECT_CALL(callback2, onSampleBundle("hd",_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle2));

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(1000));

	EXPECT_NO_THROW(at1.start());
	EXPECT_NO_THROW(at2.start());

	runTimer.wait();
	
	EXPECT_NO_THROW(at1.stop());
	EXPECT_NO_THROW(at2.stop());
	
	GT_PRINTF("Thread1: received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles1, bundle1.getSamplesNum(), bundle1.getLength(), bundle1[0].size());
	GT_PRINTF("Thread2: received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles2, bundle2.getSamplesNum(), bundle2.getLength(), bundle2[0].size());
}

TEST(TestAudioThread, TestRestart)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("g722");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	boost::shared_ptr<AudioBundlePacket> bundle(boost::make_shared<AudioBundlePacket>(wire_length));
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;
	uint64_t bundleNo = 0;

	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle = [wire_length, &bundle, &nBundles, &bundleNo](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		bundle->swap(*b);
		nBundles++;
		if (bundleNo) EXPECT_LT(bundleNo, n);
		bundleNo = n;
		EXPECT_LT(0, b->getRemainingSpace());
		EXPECT_GE(wire_length, b->getRemainingSpace());
	};

	EXPECT_CALL(callback, onSampleBundle("g722", _, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle));
	
	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(500));

	EXPECT_NO_THROW(at.start());
	EXPECT_ANY_THROW(at.start());

	runTimer.wait();
	
	EXPECT_NO_THROW(at.stop());
	int callNo = 0;
	boost::function<void(std::string, uint64_t, boost::shared_ptr<AudioBundlePacket>)> onBundle2 = [&callNo](std::string, uint64_t n, boost::shared_ptr<AudioBundlePacket> b){
		if (!callNo++) EXPECT_EQ(0,n);
	};

	EXPECT_CALL(callback, onSampleBundle("g722", _, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onBundle2));

	runTimer.expires_from_now(boost::posix_time::milliseconds(500));

	EXPECT_NO_THROW(at.start());
	EXPECT_ANY_THROW(at.start());

	runTimer.wait();
}
#endif

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
