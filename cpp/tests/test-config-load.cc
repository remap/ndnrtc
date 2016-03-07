// 
// test-config-load.cc
//
//  Created by Peter Gusev on 02 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "client/src/config.h"

#define TEST_CONFIG_FILE "tests/default.cfg"
#define TEST_CONFIG_FILE_BAD1 "tests/sample-bad1.cfg"

using namespace std;

TEST(TestConfigLoad, LoadGeneral)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params));
	EXPECT_EQ(params.getGeneralParameters().loggingLevel_, ndnlog::NdnLoggerDetailLevelDefault);
	EXPECT_EQ(params.getGeneralParameters().logFile_, "ndnrtc-client.log");
	EXPECT_EQ(params.getGeneralParameters().logPath_, "/tmp");
	EXPECT_TRUE(params.getGeneralParameters().useFec_);
	EXPECT_TRUE(params.getGeneralParameters().useAvSync_);
	EXPECT_TRUE(params.getGeneralParameters().useRtx_);
	EXPECT_TRUE(params.getGeneralParameters().skipIncomplete_);
	EXPECT_EQ(params.getGeneralParameters().host_, "localhost");
	EXPECT_EQ(params.getGeneralParameters().portNum_, 6363);
}

TEST(TestConfigLoad, LoadGeneralBad)
{
	string fileName(TEST_CONFIG_FILE_BAD1);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	ClientParams params;

	ASSERT_NE(0, loadParamsFromFile(fileName, params));
}

TEST(TestConfigLoad, LoadConsumerParams)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params));
	EXPECT_EQ(2000, params.getConsumerParams().generalAudioParams_.interestLifetime_);
	EXPECT_EQ(150, params.getConsumerParams().generalAudioParams_.bufferSlotsNum_);
	EXPECT_EQ(150, params.getConsumerParams().generalAudioParams_.jitterSizeMs_);
	EXPECT_EQ(4000, params.getConsumerParams().generalAudioParams_.slotSize_);
	EXPECT_EQ(2000, params.getConsumerParams().generalVideoParams_.interestLifetime_);
	EXPECT_EQ(200, params.getConsumerParams().generalVideoParams_.bufferSlotsNum_);
	EXPECT_EQ(150, params.getConsumerParams().generalVideoParams_.jitterSizeMs_);
	EXPECT_EQ(16000, params.getConsumerParams().generalVideoParams_.slotSize_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_.size());
	EXPECT_EQ("buffer", params.getConsumerParams().statGatheringParams_[0].statFileName_);
	EXPECT_EQ("playback", params.getConsumerParams().statGatheringParams_[1].statFileName_);
	EXPECT_EQ("play", params.getConsumerParams().statGatheringParams_[2].statFileName_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[0].getStats().size());
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[1].getStats().size());
	EXPECT_EQ(4, params.getConsumerParams().statGatheringParams_[2].getStats().size());

	EXPECT_EQ(4, params.getConsumerParams().fetchedStreams_.size());
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeVideo, params.getConsumerParams().fetchedStreams_[0].type_);
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeVideo, params.getConsumerParams().fetchedStreams_[1].type_);
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeAudio, params.getConsumerParams().fetchedStreams_[2].type_);
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeAudio, params.getConsumerParams().fetchedStreams_[3].type_);
	EXPECT_EQ("/ndn/edu/ucla/remap/ndnrtc/user/clientB", params.getConsumerParams().fetchedStreams_[0].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/ndnrtc/user/clientC", params.getConsumerParams().fetchedStreams_[1].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/ndnrtc/user/clientB", params.getConsumerParams().fetchedStreams_[2].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/ndnrtc/user/clientC", params.getConsumerParams().fetchedStreams_[3].sessionPrefix_);
	EXPECT_EQ("clientB-camera.yuv", params.getConsumerParams().fetchedStreams_[0].streamSink_);
	EXPECT_EQ("clientC-camera.yuv", params.getConsumerParams().fetchedStreams_[1].streamSink_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[2].streamSink_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[3].streamSink_);
	EXPECT_EQ("low", params.getConsumerParams().fetchedStreams_[0].threadToFetch_);
	EXPECT_EQ("mid", params.getConsumerParams().fetchedStreams_[1].threadToFetch_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[2].threadToFetch_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[3].threadToFetch_);
	EXPECT_EQ(2, params.getConsumerParams().fetchedStreams_[0].getThreadNum());
	EXPECT_EQ(2, params.getConsumerParams().fetchedStreams_[1].getThreadNum());
	EXPECT_EQ(1, params.getConsumerParams().fetchedStreams_[2].getThreadNum());
	EXPECT_EQ(1, params.getConsumerParams().fetchedStreams_[3].getThreadNum());
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[0].streamName_);
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[1].streamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[2].streamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[3].streamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[0].synchronizedStreamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[1].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[2].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[3].synchronizedStreamName_);
	EXPECT_EQ(1000, params.getConsumerParams().fetchedStreams_[0].producerParams_.segmentSize_);
	EXPECT_EQ(1000, params.getConsumerParams().fetchedStreams_[1].producerParams_.segmentSize_);
	EXPECT_EQ(1000, params.getConsumerParams().fetchedStreams_[2].producerParams_.segmentSize_);
	EXPECT_EQ(1000, params.getConsumerParams().fetchedStreams_[3].producerParams_.segmentSize_);
	EXPECT_EQ("low", params.getConsumerParams().fetchedStreams_[0].getMediaThread(0)->threadName_);
	EXPECT_EQ("hi", params.getConsumerParams().fetchedStreams_[0].getMediaThread(1)->threadName_);
	EXPECT_EQ("mid", params.getConsumerParams().fetchedStreams_[1].getMediaThread(0)->threadName_);
	EXPECT_EQ("low", params.getConsumerParams().fetchedStreams_[1].getMediaThread(1)->threadName_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[2].getMediaThread(0)->threadName_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[3].getMediaThread(0)->threadName_);
	EXPECT_EQ(720, params.getConsumerParams().fetchedStreams_[0].getVideoThread(0)->coderParams_.encodeWidth_);
	EXPECT_EQ(405, params.getConsumerParams().fetchedStreams_[0].getVideoThread(0)->coderParams_.encodeHeight_);
}


TEST(TestConfigLoad, LoadProducerParams)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params));

	EXPECT_EQ("clientA", params.getProducerParams().username_);
	EXPECT_EQ("/ndn/edu/ucla/remap", params.getProducerParams().prefix_);
	EXPECT_EQ(3, params.getProducerParams().publishedStreams_.size());
	EXPECT_EQ("camera.yuv", params.getProducerParams().publishedStreams_[0].source_);
	EXPECT_EQ("desktop.yuv", params.getProducerParams().publishedStreams_[1].source_);
	EXPECT_EQ("", params.getProducerParams().publishedStreams_[2].source_);
	EXPECT_EQ(2, params.getProducerParams().publishedStreams_[0].getThreadNum());
	EXPECT_EQ(2, params.getProducerParams().publishedStreams_[1].getThreadNum());
	EXPECT_EQ(1, params.getProducerParams().publishedStreams_[2].getThreadNum());
	EXPECT_EQ("camera", params.getProducerParams().publishedStreams_[0].streamName_);
	EXPECT_EQ("desktop", params.getProducerParams().publishedStreams_[1].streamName_);
	EXPECT_EQ("sound", params.getProducerParams().publishedStreams_[2].streamName_);
}

TEST(TestConfigLoad, LoadAndOutput)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params));

	stringstream ss;
	ss << params;

	EXPECT_EQ(
		"-general:\n"
		"log level: INFO; log file: ndnrtc-client.log (at /tmp); RTX: ON; FEC: ON; "
		"A/V Sync: ON; Skipping incomplete frames: ON; Host: localhost; Port #: 6363\n"
		"-consuming:\n"
		"general audio: interest lifetime: 2000 ms; jitter size: 150 ms; "
		"buffer size: 150 slots; slot size: 4000 bytes\n"
		"general video: interest lifetime: 2000 ms; jitter size: 150 ms; "
		"buffer size: 200 slots; slot size: 16000 bytes\n"
		"stat gathering:\n"
		"stat file: buffer.stat; stats: (jitterPlay, jitterTar, dArr)\n"
		"stat file: playback.stat; stats: (framesAcq, lambdaD, drdPrime)\n"
		"stat file: play.stat; stats: (lambdaD, drdPrime, jitterTar, dArr)\n"
		"fetching:\n"
		"[0: stream sink: clientB-camera.yuv; thread to fetch: low; session prefix: "
		"/ndn/edu/ucla/remap/ndnrtc/user/clientB; name: camera (video); synced to:"
		" sound; seg size: 1000 bytes; freshness: 0 ms; no device; 2 threads:\n"
		"[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 720x405; Drop: NO]\n"
		"[1: name: hi; 30FPS; GOP: 30; Start bitrate: 3000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1920x1080; Drop: NO]\n"
		"]\n"
		"[1: stream sink: clientC-camera.yuv; thread to fetch: mid; session prefix: "
		"/ndn/edu/ucla/remap/ndnrtc/user/clientC; name: camera (video); "
		"synced to: sound; seg size: 1000 bytes; freshness: 0 ms; no device; 2 threads:\n"
		"[0: name: mid; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1080x720; Drop: NO]\n"
		"[1: name: low; 30FPS; GOP: 30; Start bitrate: 300 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 352x288; Drop: NO]\n"
		"]\n"
		"[2: stream sink: ; thread to fetch: pcmu; session prefix: "
		"/ndn/edu/ucla/remap/ndnrtc/user/clientB; name: sound (audio); synced to:"
		" ; seg size: 1000 bytes; freshness: 0 ms; no device; 1 threads:\n"
		"[0: name: pcmu]\n"
		"]\n"
		"[3: stream sink: ; thread to fetch: pcmu; session prefix: "
		"/ndn/edu/ucla/remap/ndnrtc/user/clientC; name: sound (audio); synced to:"
		" ; seg size: 1000 bytes; freshness: 0 ms; no device; 1 threads:\n"
		"[0: name: pcmu]\n"
		"]\n"
		"-producing:\n"
		"username: clientA; prefix: /ndn/edu/ucla/remap;\n"
		"--0:\n"
		"stream source: camera.yuv; session prefix: /ndn/edu/ucla/remap/ndnrtc/user/clientA; name: camera (video); "
		"synced to: sound; seg size: 1000 bytes; freshness: 2000 ms; no device; 2 threads:\n"
		"[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 720x405; Drop: YES]\n"
		"[1: name: hi; 30FPS; GOP: 30; Start bitrate: 3000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1920x1080; Drop: YES]\n"
		"--1:\n"
		"stream source: desktop.yuv; session prefix: /ndn/edu/ucla/remap/ndnrtc/user/clientA; name: desktop (video); "
		"synced to: sound; seg size: 1000 bytes; freshness: 2000 ms; no device; 2 threads:\n"
		"[0: name: mid; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1080x720; Drop: YES]\n"
		"[1: name: low; 30FPS; GOP: 30; Start bitrate: 300 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 352x288; Drop: YES]\n"
		"--2:\n"
		"stream source: ; session prefix: /ndn/edu/ucla/remap/ndnrtc/user/clientA; name: sound (audio); "
		"synced to: ; seg size: 1000 bytes; freshness: 2000 ms; "
		"no device; 1 threads:\n"
		"[0: name: pcmu]\n",
		ss.str());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}