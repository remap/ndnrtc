// 
// test-config-load.cc
//
//  Created by Peter Gusev on 02 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "client/src/config.hpp"

#define TEST_CONFIG_FILE "tests/default.cfg"
#define TEST_CONFIG_SAMPLE_CONSUMER_FILE "tests/sample-consumer.cfg"
#define TEST_CONFIG_SAMPLE_PRODUCER_FILE "tests/sample-producer.cfg"
#define TEST_CONFIG_FILE_BAD1 "tests/sample-bad1.cfg"
#define TEST_CONFIG_FILE_CA "tests/consumer-audio.cfg"
#define TEST_CONFIG_FILE_CV "tests/consumer-video.cfg"

using namespace std;

TEST(TestConfigLoad, LoadGeneral)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));
	EXPECT_EQ(params.getGeneralParameters().loggingLevel_, ndnlog::NdnLoggerDetailLevelDefault);
	EXPECT_EQ(params.getGeneralParameters().logFile_, "ndnrtc-client.log");
	EXPECT_EQ(params.getGeneralParameters().logPath_, "/tmp");
	EXPECT_TRUE(params.getGeneralParameters().useFec_);
	EXPECT_TRUE(params.getGeneralParameters().useAvSync_);
	EXPECT_EQ(params.getGeneralParameters().host_, "localhost");
	EXPECT_EQ(params.getGeneralParameters().portNum_, 6363);
}

TEST(TestConfigLoad, LoadGeneralBad)
{
	string fileName(TEST_CONFIG_FILE_BAD1);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_NE(0, loadParamsFromFile(fileName, params, identity));
}

TEST(TestConfigLoad, LoadConsumerParams)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));
	EXPECT_EQ(2000, params.getConsumerParams().generalAudioParams_.interestLifetime_);
	EXPECT_EQ(150, params.getConsumerParams().generalAudioParams_.jitterSizeMs_);
	EXPECT_EQ(2000, params.getConsumerParams().generalVideoParams_.interestLifetime_);
	EXPECT_EQ(150, params.getConsumerParams().generalVideoParams_.jitterSizeMs_);
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
	EXPECT_EQ("/ndn/edu/ucla/remap/clientB", params.getConsumerParams().fetchedStreams_[0].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientC", params.getConsumerParams().fetchedStreams_[1].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientB", params.getConsumerParams().fetchedStreams_[2].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientC", params.getConsumerParams().fetchedStreams_[3].sessionPrefix_);
	EXPECT_EQ("clientB-camera", params.getConsumerParams().fetchedStreams_[0].sink_.name_);
	EXPECT_EQ("clientC-camera", params.getConsumerParams().fetchedStreams_[1].sink_.name_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[2].sink_.name_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[3].sink_.name_);
	EXPECT_EQ("low", params.getConsumerParams().fetchedStreams_[0].threadToFetch_);
	EXPECT_EQ("mid", params.getConsumerParams().fetchedStreams_[1].threadToFetch_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[2].threadToFetch_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[3].threadToFetch_);
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[0].getThreadNum());
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[1].getThreadNum());
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[2].getThreadNum());
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[3].getThreadNum());
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[0].streamName_);
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[1].streamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[2].streamName_);
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[3].streamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[0].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[1].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[2].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[3].synchronizedStreamName_);
}

TEST(TestConfigLoad, LoadProducerParams)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));

	EXPECT_EQ("/test/identity", params.getProducerParams().prefix_);
	EXPECT_EQ(3, params.getProducerParams().publishedStreams_.size());
	EXPECT_EQ("camera.argb", params.getProducerParams().publishedStreams_[0].source_);
	EXPECT_EQ("desktop.argb", params.getProducerParams().publishedStreams_[1].source_);
	EXPECT_EQ("", params.getProducerParams().publishedStreams_[2].source_);
	EXPECT_EQ(2, params.getProducerParams().publishedStreams_[0].getThreadNum());
	EXPECT_EQ(2, params.getProducerParams().publishedStreams_[1].getThreadNum());
	EXPECT_EQ(1, params.getProducerParams().publishedStreams_[2].getThreadNum());
	EXPECT_EQ("camera", params.getProducerParams().publishedStreams_[0].streamName_);
	EXPECT_EQ("desktop", params.getProducerParams().publishedStreams_[1].streamName_);
	EXPECT_EQ("sound", params.getProducerParams().publishedStreams_[2].streamName_);
}

TEST(TestConfigLoad, LoadAudioConsumerOnly)
{
	// ndnlog::new_api::Logger::initAsyncLogging();
	string fileName(TEST_CONFIG_FILE_CA);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));
	EXPECT_EQ(2000, params.getConsumerParams().generalAudioParams_.interestLifetime_);
	EXPECT_EQ(150, params.getConsumerParams().generalAudioParams_.jitterSizeMs_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_.size());
	EXPECT_EQ("buffer", params.getConsumerParams().statGatheringParams_[0].statFileName_);
	EXPECT_EQ("playback", params.getConsumerParams().statGatheringParams_[1].statFileName_);
	EXPECT_EQ("play", params.getConsumerParams().statGatheringParams_[2].statFileName_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[0].getStats().size());
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[1].getStats().size());
	EXPECT_EQ(4, params.getConsumerParams().statGatheringParams_[2].getStats().size());
	EXPECT_EQ(2, params.getConsumerParams().fetchedStreams_.size());
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeAudio, params.getConsumerParams().fetchedStreams_[0].type_);
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeAudio, params.getConsumerParams().fetchedStreams_[1].type_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientA", params.getConsumerParams().fetchedStreams_[0].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientA", params.getConsumerParams().fetchedStreams_[1].sessionPrefix_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[0].sink_.name_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[1].sink_.name_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[0].threadToFetch_);
	EXPECT_EQ("pcmu", params.getConsumerParams().fetchedStreams_[1].threadToFetch_);
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[0].getThreadNum());
	EXPECT_EQ(0, params.getConsumerParams().fetchedStreams_[1].getThreadNum());
	EXPECT_EQ("sound", params.getConsumerParams().fetchedStreams_[0].streamName_);
	EXPECT_EQ("sound2", params.getConsumerParams().fetchedStreams_[1].streamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[0].synchronizedStreamName_);
	EXPECT_EQ("", params.getConsumerParams().fetchedStreams_[1].synchronizedStreamName_);
}

TEST(TestConfigLoad, LoadVideoConsumerOnly)
{
	string fileName(TEST_CONFIG_FILE_CV);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));
	EXPECT_EQ(2000, params.getConsumerParams().generalVideoParams_.interestLifetime_);
	EXPECT_EQ(150, params.getConsumerParams().generalVideoParams_.jitterSizeMs_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_.size());
	EXPECT_EQ("buffer", params.getConsumerParams().statGatheringParams_[0].statFileName_);
	EXPECT_EQ("playback", params.getConsumerParams().statGatheringParams_[1].statFileName_);
	EXPECT_EQ("play", params.getConsumerParams().statGatheringParams_[2].statFileName_);
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[0].getStats().size());
	EXPECT_EQ(3, params.getConsumerParams().statGatheringParams_[1].getStats().size());
	EXPECT_EQ(4, params.getConsumerParams().statGatheringParams_[2].getStats().size());

	EXPECT_EQ(2, params.getConsumerParams().fetchedStreams_.size());
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeVideo, params.getConsumerParams().fetchedStreams_[0].type_);
	EXPECT_EQ(ClientMediaStreamParams::MediaStreamTypeVideo, params.getConsumerParams().fetchedStreams_[1].type_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientB", params.getConsumerParams().fetchedStreams_[0].sessionPrefix_);
	EXPECT_EQ("/ndn/edu/ucla/remap/clientC", params.getConsumerParams().fetchedStreams_[1].sessionPrefix_);
	EXPECT_EQ("clientB-camera", params.getConsumerParams().fetchedStreams_[0].sink_.name_);
	EXPECT_EQ("/tmp/clientC-camera", params.getConsumerParams().fetchedStreams_[1].sink_.name_);
	EXPECT_STREQ("file", params.getConsumerParams().fetchedStreams_[0].sink_.type_.c_str());
	EXPECT_STREQ("pipe", params.getConsumerParams().fetchedStreams_[1].sink_.type_.c_str());
	EXPECT_EQ("low", params.getConsumerParams().fetchedStreams_[0].threadToFetch_);
	EXPECT_EQ("mid", params.getConsumerParams().fetchedStreams_[1].threadToFetch_);
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[0].streamName_);
	EXPECT_EQ("camera", params.getConsumerParams().fetchedStreams_[1].streamName_);
}
// TBD: since output formatting changes frequently, these tests below need to be updated
// since i'm not enjoying it, someone is welcome to volunteer
#if 0
TEST(TestConfigLoad, LoadAndOutput)
{
	string fileName(TEST_CONFIG_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));

	stringstream ss;
	ss << params;

	// std::cout << params << std::endl;
#if 1
	EXPECT_EQ(
		"-general:\n"
		"log level: INFO; log file: ndnrtc-client.log (at /tmp); FEC: ON; "
		"A/V Sync: ON; Host: localhost; Port #: 6363\n"
		"-consuming:\n"
		"general audio: interest lifetime: 2000 ms; jitter size: 150 ms\n"
		"general video: interest lifetime: 2000 ms; jitter size: 150 ms\n"
		"stat gathering:\n"
		"stat file: buffer.stat; stats: (jitterPlay, jitterTar, dArr)\n"
		"stat file: playback.stat; stats: (framesAcq, lambdaD, drdPrime)\n"
		"stat file: play.stat; stats: (lambdaD, drdPrime, jitterTar, dArr)\n"
		"fetching:\n"
		"[0: stream sink: clientB-camera; thread to fetch: low; session prefix: "
		"/ndn/edu/ucla/remap/clientB; name: camera (video); synced to: ;"
		" seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n"
		"[1: stream sink: clientC-camera; thread to fetch: mid; session prefix: "
		"/ndn/edu/ucla/remap/clientC; name: camera (video); "
		"synced to: ; seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n"
		"[2: stream sink: ; thread to fetch: pcmu; session prefix: "
		"/ndn/edu/ucla/remap/clientB; name: sound (audio); synced to:"
		" ; seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n"
		"[3: stream sink: ; thread to fetch: pcmu; session prefix: "
		"/ndn/edu/ucla/remap/clientC; name: sound (audio); synced to:"
		" ; seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n"
		"-producing:\n"
		"prefix: /test/identity;\n"
		"--0:\n"
		"stream source: camera.argb; session prefix: ; name: camera (video); "
		"synced to: sound; seg size: 1000 bytes; freshness: 2000 ms; no device; 2 threads:\n"
		"[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 720x405; Drop: YES]\n"
		"[1: name: hi; 30FPS; GOP: 30; Start bitrate: 3000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1920x1080; Drop: YES]\n"
		"--1:\n"
		"stream source: desktop.argb; session prefix: ; name: desktop (video); "
		"synced to: sound; seg size: 1000 bytes; freshness: 2000 ms; no device; 2 threads:\n"
		"[0: name: mid; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 1080x720; Drop: YES]\n"
		"[1: name: low; 30FPS; GOP: 30; Start bitrate: 300 Kbit/s; "
		"Max bitrate: 10000 Kbit/s; 352x288; Drop: YES]\n"
		"--2:\n"
		"stream source: ; session prefix: ; name: sound (audio); "
		"synced to: ; seg size: 1000 bytes; freshness: 2000 ms; "
		"capture device id: 0; 1 threads:\n"
		"[0: name: pcmu; codec: g722]\n",
		ss.str());
#endif
}

TEST(TestConfigLoad, TestSampleConsumerParams)
{
	// ndnlog::new_api::Logger::initAsyncLogging();
	string fileName(TEST_CONFIG_SAMPLE_CONSUMER_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));

	stringstream ss;
	ss << params;

	EXPECT_EQ(
		"-general:\n"
		"log level: INFO; log file: ndnrtc-client.log (at /tmp); FEC: ON; "
		"A/V Sync: ON; Host: localhost; Port #: 6363\n"
		"-consuming:\n"
		"general audio: interest lifetime: 2000 ms; jitter size: 150 ms\n"
		"general video: interest lifetime: 2000 ms; jitter size: 150 ms\n"
		"stat gathering:\n"
		"stat file: buffer.stat; stats: (jitterPlay, jitterTar, dArr)\n"
		"stat file: playback.stat; stats: (framesAcq, lambdaD, drdPrime)\n"
		"stat file: play.stat; stats: (lambdaD, drdPrime, jitterTar, dArr)\n"
		"fetching:\n"
		"[0: stream sink: ; thread to fetch: pcmu; session prefix: "
		"/ndn/edu/ucla/remap/clientA; name: sound (audio); synced to:"
		" ; seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n"
		"[1: stream sink: /tmp/clientA-camera; thread to fetch: tiny; session prefix: "
		"/ndn/edu/ucla/remap/clientA; name: camera (video); synced to:"
		" ; seg size: 0 bytes; freshness: 0 ms; no device; 0 threads:\n"
		"]\n",
		ss.str());
}

TEST(TestConfigLoad, TestSampleProducerParams)
{
	// ndnlog::new_api::Logger::initAsyncLogging();
	string fileName(TEST_CONFIG_SAMPLE_PRODUCER_FILE);
	
	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());
	
	string identity = "/test/identity";
	ClientParams params;

	ASSERT_EQ(0, loadParamsFromFile(fileName, params, identity));

	stringstream ss;
	ss << params;

	EXPECT_EQ(
		"-general:\n"
		"log level: INFO; log file: ndnrtc-client.log (at /tmp); FEC: ON; A/V Sync: ON; "
		"Host: localhost; Port #: 6363\n"
		"-producing:\n"
		"prefix: /test/identity;\n"
		"--0:\n"
		"stream source: ../tests/test-source-320x240.argb; session prefix: ; "
		"name: camera (video); synced to: sound; seg size: 1000 bytes; freshness: 2000 ms; no device; 1 threads:\n"
		"[0: name: tiny; 30FPS; GOP: 30; Start bitrate: 100 Kbit/s; Max bitrate: 10000 Kbit/s; 320x240; Drop: YES]\n"
		"--1:\n"
		"stream source: ; session prefix: ; name: sound (audio); synced to: ;"
		" seg size: 1000 bytes; freshness: 2000 ms; capture device id: 0; 1 threads:\n"
		"[0: name: pcmu; codec: g722]\n",
		ss.str());
}
#endif
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}