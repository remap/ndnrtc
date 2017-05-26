// 
// test-audio-capturer.cc
//
//  Created by Peter Gusev on 17 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/chrono.hpp>

#include "gtest/gtest.h"

#include "tests-helpers.hpp"
#include "src/audio-capturer.hpp"
#include "mock-objects/audio-sample-consumer-mock.hpp"

using namespace ::testing;
using namespace std;
using namespace ndnrtc;
using namespace boost::chrono;

TEST(TestAudioCapturer, TestAudioDevices)
{
	std::vector<std::pair<string,string>> recordingDevices, playoutDevices;

	recordingDevices = AudioCapturer::getRecordingDevices();
	playoutDevices = AudioCapturer::getPlayoutDevices();

	EXPECT_LT(0, recordingDevices.size());
	EXPECT_LT(0, playoutDevices.size());
}

TEST(TestAudioCapturer, TestCreate)
{
	{
		MockAudioSampleConsumer sampleConsumer;
		EXPECT_NO_THROW(AudioCapturer capturer(0, &sampleConsumer));
	}
	{
		MockAudioSampleConsumer sampleConsumer;
		EXPECT_NO_THROW(AudioCapturer capturer(0, &sampleConsumer, 
			WebrtcAudioChannel::Codec::Opus));
	}
}

TEST(TestAudioCapturer, TestCreateThrow)
{
	MockAudioSampleConsumer sampleConsumer;
	EXPECT_ANY_THROW(AudioCapturer(999, &sampleConsumer));
}

// this code may be used for checking thread deadlocks when stopping capturer
#if 0
TEST(TestAudioCapturer, TestDestructorThreading)
{
	MockAudioSampleConsumer sampleConsumer;

	EXPECT_CALL(sampleConsumer, onDeliverRtpFrame(_,_))
		.Times(AtLeast(1));
	EXPECT_CALL(sampleConsumer, onDeliverRtcpFrame(_,_))
		.Times(AtLeast(0));

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	for (int i = 0; i < 100; ++i)
	{
		AudioCapturer capturer(0, &sampleConsumer, WebrtcAudioChannel::Codec::Opus);
		capturer.startCapture();
		runTimer.expires_from_now(boost::posix_time::milliseconds(100));
		runTimer.wait();
	}
}
#endif

TEST(TestAudioCapturer, TestCapture)
{
	MockAudioSampleConsumer sampleConsumer;
	AudioCapturer capturer(0, &sampleConsumer);

	int nRtp = 0, nRtcp = 0, rtpLen = 0, rtcpLen = 0;
	high_resolution_clock::time_point callbackTs;
	boost::function<void(unsigned int, unsigned char*)> onRtp = [&nRtp, &callbackTs, &rtpLen](unsigned int len, unsigned char*){
		nRtp++;
		rtpLen = len;
		callbackTs = high_resolution_clock::now();
	};
	boost::function<void(unsigned int, unsigned char*)> onRtcp = [&nRtcp, &rtcpLen, &callbackTs](unsigned int len, unsigned char*){
		nRtcp++;
		rtcpLen = len;
		callbackTs = high_resolution_clock::now();
	};

	EXPECT_CALL(sampleConsumer, onDeliverRtpFrame(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onRtp));
	EXPECT_CALL(sampleConsumer, onDeliverRtcpFrame(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onRtcp));

	EXPECT_NO_THROW(capturer.startCapture());
	EXPECT_ANY_THROW(capturer.startCapture());

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(3000));
	runTimer.wait();

	capturer.stopCapture();
	ASSERT_GT(high_resolution_clock::now(), callbackTs);

	GT_PRINTF("Default audio codec: %d RTP packets (%d bytes each), %d RTCP packets (%d bytes each)\n",
		nRtp, rtpLen, nRtcp, rtcpLen);
}

TEST(TestAudioCapturer, TestCaptureOpusCodec)
{
	MockAudioSampleConsumer sampleConsumer;
	AudioCapturer capturer(0, &sampleConsumer, WebrtcAudioChannel::Codec::Opus);

	int nRtp = 0, nRtcp = 0, rtpLen = 0, rtcpLen = 0;
	high_resolution_clock::time_point callbackTs;
	boost::function<void(unsigned int, unsigned char*)> onRtp = [&nRtp, &callbackTs, &rtpLen](unsigned int len, unsigned char*){
		nRtp++;
		rtpLen = len;
		callbackTs = high_resolution_clock::now();
	};
	boost::function<void(unsigned int, unsigned char*)> onRtcp = [&nRtcp, &rtcpLen, &callbackTs](unsigned int len, unsigned char*){
		nRtcp++;
		rtcpLen = len;
		callbackTs = high_resolution_clock::now();
	};

	EXPECT_CALL(sampleConsumer, onDeliverRtpFrame(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onRtp));
	EXPECT_CALL(sampleConsumer, onDeliverRtcpFrame(_,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(onRtcp));

	EXPECT_NO_THROW(capturer.startCapture());
	EXPECT_ANY_THROW(capturer.startCapture());

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(3000));
	runTimer.wait();

	capturer.stopCapture();
	ASSERT_GT(high_resolution_clock::now(), callbackTs);

	GT_PRINTF("Default audio codec: %d RTP packets (%d bytes each), %d RTCP packets (%d bytes each)\n",
		nRtp, rtpLen, nRtcp, rtcpLen);
}

TEST(TestAudioCapturer, TestMultipleCaptureDifferentCodecs)
{
	MockAudioSampleConsumer sampleConsumer1;
	MockAudioSampleConsumer sampleConsumer2;

	EXPECT_CALL(sampleConsumer1, onDeliverRtpFrame(_,_))
		.Times(AtLeast(1));
	EXPECT_CALL(sampleConsumer1, onDeliverRtcpFrame(_,_))
		.Times(AtLeast(1));
	EXPECT_CALL(sampleConsumer2, onDeliverRtpFrame(_,_))
		.Times(AtLeast(1));
	EXPECT_CALL(sampleConsumer2, onDeliverRtcpFrame(_,_))
		.Times(AtLeast(1));

	AudioCapturer* g722capturer; 
	EXPECT_NO_THROW(g722capturer = new AudioCapturer(0,&sampleConsumer1));

	AudioCapturer* opusCapturer; 
	EXPECT_NO_THROW(opusCapturer = new AudioCapturer(0,&sampleConsumer2));

	EXPECT_NO_THROW(g722capturer->startCapture());
	EXPECT_NO_THROW(opusCapturer->startCapture());

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	runTimer.expires_from_now(boost::posix_time::milliseconds(3000));
	runTimer.wait();

	delete g722capturer;
	delete opusCapturer;
}

TEST(TestAudioCapturer, TestMultipleCapturerDifferentDevices)
{
	std::vector<std::pair<string,string>> recordingDevices = AudioCapturer::getRecordingDevices();

	if (recordingDevices.size())
	{
		std::vector<boost::shared_ptr<uint8_t>> rtp1;
		std::vector<int> lengths1;
		std::vector<boost::shared_ptr<uint8_t>> rtp2;
		std::vector<int> lengths2;
		MockAudioSampleConsumer sampleConsumer1;
		MockAudioSampleConsumer sampleConsumer2;

		int rtp1Len = 0, rtp2Len = 0;
		boost::function<void(unsigned int, unsigned char*)> onRtp1 = [&rtp1, &rtp1Len, &lengths1](unsigned int len, unsigned char* data){
			rtp1Len = len;
			boost::shared_ptr<uint8_t> rtp(new uint8_t[len]);
			memcpy(rtp.get(), data, len);
			rtp1.push_back(rtp);
			lengths1.push_back(len);
		};
		boost::function<void(unsigned int, unsigned char*)> onRtp2 = [&rtp2, &rtp2Len, &lengths2](unsigned int len, unsigned char* data){
			rtp2Len = len;
			boost::shared_ptr<uint8_t> rtp(new uint8_t[len]);
			memcpy(rtp.get(), data, len);
			rtp2.push_back(rtp);
			lengths2.push_back(len);
		};

		EXPECT_CALL(sampleConsumer1, onDeliverRtpFrame(_,_))
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke(onRtp1));
		EXPECT_CALL(sampleConsumer1, onDeliverRtcpFrame(_,_))
			.Times(AtLeast(1));
		EXPECT_CALL(sampleConsumer2, onDeliverRtpFrame(_,_))
			.Times(AtLeast(1))
			.WillRepeatedly(Invoke(onRtp2));
		EXPECT_CALL(sampleConsumer2, onDeliverRtcpFrame(_,_))
			.Times(AtLeast(1));

		boost::asio::io_service io;
		boost::asio::deadline_timer runTimer(io);

		AudioCapturer *c1;
		EXPECT_NO_THROW(c1 = new AudioCapturer(0, &sampleConsumer1));

		c1->startCapture();

		AudioCapturer *c2;
		EXPECT_NO_THROW(c2 = new AudioCapturer(recordingDevices.size()-1, &sampleConsumer2, WebrtcAudioChannel::Codec::Opus));

		c2->startCapture();

		runTimer.expires_from_now(boost::posix_time::milliseconds(3000));
		runTimer.wait();

		// now, gathered rtp packets should differ
		for (int i = 0; i < min(rtp1.size(), rtp2.size()); ++i)
		{
			boost::shared_ptr<uint8_t> p1 = rtp1[i];
			boost::shared_ptr<uint8_t> p2 = rtp2[i];
			bool identical = true;
			for (int j = 0; j < min(rtp1Len, rtp2Len) && identical; ++j)
				identical = (p1.get()[j] == p2.get()[j]);
			EXPECT_FALSE(identical);
		}

		delete c1;
		delete c2;
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
