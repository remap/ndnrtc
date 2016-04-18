// 
// test-media-thread.cc
//
//  Created by Peter Gusev on 08 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread/future.hpp>
#include <boost/asio.hpp>
#include <stdlib.h>

#include "gtest/gtest.h"

#include "tests-helpers.h"
#include "src/video-thread.h"
#include "src/audio-thread.h"
#include "mock-objects/audio-thread-callback-mock.h"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

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
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	webrtc::I420VideoFrame convertedFrame;

	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise
	{
		// make conversion to I420
		const webrtc::VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(webrtc::kVideoARGB);
		int stride_y = width;
		int stride_uv = (width + 1) / 2;
		int target_width = width;
		int target_height = height;
	
		convertedFrame.CreateEmptyFrame(target_width,
			abs(target_height), stride_y, stride_uv, stride_uv);
		ConvertToI420(commonVideoType, frameBuffer, 0, 0,  // No cropping
						width, height, frameSize, webrtc::kVideoRotation_0, &convertedFrame);
	}

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
		EXPECT_EQ(webrtc::kKeyFrame, vf->getFrame()._frameType);
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
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	webrtc::I420VideoFrame convertedFrame;

	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise
	{
		// make conversion to I420
		const webrtc::VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(webrtc::kVideoARGB);
		int stride_y = width;
		int stride_uv = (width + 1) / 2;
		int target_width = width;
		int target_height = height;
	
		convertedFrame.CreateEmptyFrame(target_width,
			abs(target_height), stride_y, stride_uv, stride_uv);
		ConvertToI420(commonVideoType, frameBuffer, 0, 0,  // No cropping
						width, height, frameSize, webrtc::kVideoRotation_0, &convertedFrame);
	}

	{ // try 1 frame to encode asynchronously
		VideoCoderParams vcp(sampleVideoCoderParams());
		vcp.startBitrate_ = 1000;
		vcp.maxBitrate_ = 1000;
		vcp.encodeWidth_ = width;
		vcp.encodeHeight_ = height;

		VideoThread vt(vcp);
#ifdef ENABLE_LOGGING
		vt.setLogger(&ndnlog::new_api::Logger::getLogger(""));
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
		EXPECT_EQ(webrtc::kKeyFrame, vf->getFrame()._frameType);
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
 	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	std::vector<webrtc::I420VideoFrame> frames;

	for (int f = 0; f < nFrames; ++f)
	{
		for (int j = 0; j < height; ++j)
			for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

		webrtc::I420VideoFrame convertedFrame;
		{
		// make conversion to I420
			const webrtc::VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(webrtc::kVideoARGB);
			int stride_y = width;
			int stride_uv = (width + 1) / 2;
			int target_width = width;
			int target_height = height;

			convertedFrame.CreateEmptyFrame(target_width,
				abs(target_height), stride_y, stride_uv, stride_uv);
			ConvertToI420(commonVideoType, frameBuffer, 0, 0,  // No cropping
				width, height, frameSize, webrtc::kVideoRotation_0, &convertedFrame);
		}
		frames.push_back(convertedFrame);
	}

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
	for (auto params:coderParams) 
	{
		videoThreads.push_back(boost::make_shared<VideoThread>(params));
		scaleFrame.push_back(boost::make_shared<FrameScaler>(params.encodeWidth_, params.encodeHeight_));

#ifdef ENABLE_LOGGING
		videoThreads.back()->setLogger(&ndnlog::new_api::Logger::getLogger(""));
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
		runTimer.wait();
	}

	for (int i = 0; i < videoThreads.size(); ++i)
	{
		EXPECT_LE(0, videoThreads[i]->getEncodedNum());
		GT_PRINTF("thread %d (%dx%d %d Kbit/s): %d encoded, %d dropped\n", 
			i, coderParams[i].encodeWidth_, coderParams[i].encodeHeight_, 
			coderParams[i].startBitrate_, 
			videoThreads[i]->getEncodedNum(), videoThreads[i]->getDroppedNum());
	}
}

TEST(TestAudioThread, TestRunOpusThread)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("mic", "opus");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	AudioBundlePacket bundle(wire_length);
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;

	boost::function<void(AudioBundlePacket& bundle)> onBundle = [wire_length, &bundle, &nBundles](AudioBundlePacket& b){
		bundle.swap(boost::move(b));
		nBundles++;
		EXPECT_LT(0, b.getRemainingSpace());
		EXPECT_GE(wire_length, b.getRemainingSpace());
	};

	EXPECT_CALL(callback, onSampleBundle(_))
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
	EXPECT_GE(wire_length, bundle.getLength());
	EXPECT_LT(1, bundle.getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle.getSamplesNum(), bundle.getLength(), bundle[0].size());
}

TEST(TestAudioThread, TestRunG722Thread)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("mic");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 1000;
	AudioBundlePacket bundle(wire_length);
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;

	boost::function<void(AudioBundlePacket& bundle)> onBundle = [wire_length, &bundle, &nBundles](AudioBundlePacket& b){
		bundle.swap(boost::move(b));
		nBundles++;
		EXPECT_LT(0, b.getRemainingSpace());
		EXPECT_GE(wire_length, b.getRemainingSpace());
	};

	EXPECT_CALL(callback, onSampleBundle(_))
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
	EXPECT_GE(wire_length, bundle.getLength());
	EXPECT_LT(1, bundle.getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle.getSamplesNum(), bundle.getLength(), bundle[0].size());
}

TEST(TestAudioThread, TestRun8kSegment)
{
	MockAudioThreadCallback callback;
	AudioThreadParams ap("mic");
	AudioCaptureParams acp;
	acp.deviceId_ = 0;
	int wire_length = 8000;
	AudioBundlePacket bundle(wire_length);
	AudioThread at(ap, acp, &callback, wire_length);
	int nBundles = 0;

	boost::function<void(AudioBundlePacket& bundle)> onBundle = [wire_length, &bundle, &nBundles](AudioBundlePacket& b){
		bundle.swap(boost::move(b));
		nBundles++;
		EXPECT_LT(0, b.getRemainingSpace());
		EXPECT_GE(wire_length, b.getRemainingSpace());
	};

	EXPECT_CALL(callback, onSampleBundle(_))
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
	EXPECT_GE(wire_length, bundle.getLength());
	EXPECT_LT(1, bundle.getSamplesNum());
	GT_PRINTF("Received %d bundles (%d samples per bundle, bundle length %d, sample size %d)\n", 
		nBundles, bundle.getSamplesNum(), bundle.getLength(), bundle[0].size());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
