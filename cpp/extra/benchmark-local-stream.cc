// 
// benchmark-local-stream.cc
//
//  Created by Peter Gusev on 16 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>

#include <boost/thread.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "gtest/gtest.h"
#include "../tests/tests-helpers.hpp"
#include "include/local-stream.hpp"
#include "include/name-components.hpp"
#include "client/src/video-source.hpp"
#include "../tests/mock-objects/external-capturer-mock.hpp"
#include "statistics.hpp"

std::string test_path = "";

using namespace ::testing;
using namespace boost;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

// #define ENABLE_LOGGING

void runProducer(std::string sourceFile, boost::shared_ptr<RawFrame> frame,
	MediaStreamParams msp, int runTimeMs, bool sign = true)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    
	boost::asio::io_service face_io;
	boost::shared_ptr<boost::asio::io_service::work> face_work(boost::make_shared<boost::asio::io_service::work>(face_io));
	boost::thread face_t([&face_io](){
		face_io.run();
	});

	boost::asio::io_service capture_io;
	boost::shared_ptr<boost::asio::io_service::work> capture_work(boost::make_shared<boost::asio::io_service::work>(capture_io));
	boost::thread capture_t([&capture_io](){
		capture_io.run();
	});

	boost::asio::io_service stat_io;
	boost::asio::deadline_timer statTimer(stat_io);
	boost::shared_ptr<boost::asio::io_service::work> stat_work(boost::make_shared<boost::asio::io_service::work>(stat_io));
	boost::thread stat_t([&stat_io](){
		stat_io.run();
	});

	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	MediaStreamSettings settings(face_io, msp);
    settings.sign_ = sign;
	settings.face_ = &face;
	settings.keyChain_ = keyChain.get();
	LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
    s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

    VideoSource source(capture_io, sourceFile, frame);
    MockExternalCapturer capturer;
    source.addCapturer(&capturer);

    int nsourced = 0;
	boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
    incomingRawFrame =[&s, &nsourced](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
        EXPECT_NO_THROW(s.incomingArgbFrame(w, h, data, size));
        nsourced++;
        return 0;
    };
    
    EXPECT_CALL(capturer, incomingArgbFrame(frame->getWidth(), frame->getHeight(), _, _))
    	.WillRepeatedly(Invoke(incomingRawFrame));

    source.start(30);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTimeMs));
    source.stop();
    face.shutdown();

    face_work.reset();
    face_io.stop();
    face_t.join();

    capture_work.reset();
    capture_io.stop();
    capture_t.join();

    stat_work.reset();
    stat_io.stop();
    stat_t.join();

    StatisticsStorage stat = s.getStatistics();
    double runTimeSec = ((double)runTimeMs/1000.);
    double bitrate = stat[Indicator::BytesPublished]*8./1000./runTimeSec;
    double rawBitrate = stat[Indicator::RawBytesPublished]*8./1000./runTimeSec;

    GT_PRINTF("cap/proc: %d/%d, enc/drop: %d/%d, pub(key): %d(%d), "
              "caprate: %.2f, pubrate:%.2f, "
              "bitrate: %.2fKbps, wirerate: %.2fKbps (%.2f%% overhead)\n",
              (int)stat[Indicator::CapturedNum], (int)stat[Indicator::ProcessedNum],
              (int)stat[Indicator::EncodedNum], (int)stat[Indicator::DroppedNum],
              (int)stat[Indicator::PublishedNum], (int)stat[Indicator::PublishedKeyNum],
              stat[Indicator::CapturedNum]/runTimeSec, stat[Indicator::ProcessedNum]/runTimeSec,
              bitrate, rawBitrate, (rawBitrate-bitrate)/bitrate*100);
    GT_PRINTF("total segments: %d, avg segment size: %.2f, segrate: %.2f, "
              "avg seg/frame: %.2f, sopnum: %d, sop/sec: %.2f, enc/sec: %.2f\n",
              (int)stat[Indicator::PublishedSegmentsNum],
              stat[Indicator::BytesPublished]/stat[Indicator::PublishedSegmentsNum],
              stat[Indicator::PublishedSegmentsNum]/runTimeSec,
              stat[Indicator::PublishedSegmentsNum]/stat[Indicator::PublishedNum],
              (int)stat[Indicator::SignNum], stat[Indicator::SignNum]/runTimeSec,
              stat[Indicator::EncodedNum]/runTimeSec);
}

unsigned int runtime = 5000;
#if 0
TEST(BenchmarkLocalStream, VideoStream320x240_300)
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("mid", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	atp.coderParams_.startBitrate_ = 300;
	msp.addMediaThread(atp);

    runProducer(test_path+"/../res/test-source-320x240.argb",
    	boost::make_shared<ArgbFrame>(320,240),
    	msp,
    	runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_500)
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("mid", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	atp.coderParams_.startBitrate_ = 500;
	msp.addMediaThread(atp);

    runProducer(test_path+"/../res/test-source-320x240.argb",
    	boost::make_shared<ArgbFrame>(320,240),
    	msp,
    	runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_700)
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("mid", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	atp.coderParams_.startBitrate_ = 700;
	msp.addMediaThread(atp);

    runProducer(test_path+"/../res/test-source-320x240.argb",
    	boost::make_shared<ArgbFrame>(320,240),
    	msp,
    	runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_1000)
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("mid", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	atp.coderParams_.startBitrate_ = 1000;
	msp.addMediaThread(atp);

    runProducer(test_path+"/../res/test-source-320x240.argb",
    	boost::make_shared<ArgbFrame>(320,240),
    	msp,
    	runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_300_500)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 320;
    atp.coderParams_.encodeHeight_ = 240;
    atp.coderParams_.startBitrate_ = 300;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 500;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-320x240.argb",
                                     boost::make_shared<ArgbFrame>(320,240),
                                     msp,
                                     runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_300_500_1000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 320;
    atp.coderParams_.encodeHeight_ = 240;
    atp.coderParams_.startBitrate_ = 300;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 500;
        msp.addMediaThread(atp);
    }

    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 1000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-320x240.argb",
                                     boost::make_shared<ArgbFrame>(320,240),
                                     msp,
                                     runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_300x2)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 320;
    atp.coderParams_.encodeHeight_ = 240;
    atp.coderParams_.startBitrate_ = 300;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 300;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-320x240.argb",
                boost::make_shared<ArgbFrame>(320,240),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream320x240_300x3)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 320;
    atp.coderParams_.encodeHeight_ = 240;
    atp.coderParams_.startBitrate_ = 300;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 300;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 240;
        atp.coderParams_.startBitrate_ = 300;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-320x240.argb",
                boost::make_shared<ArgbFrame>(320,240),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_500)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("mid", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_1000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("mid", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 1000;
    msp.addMediaThread(atp);
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_1500)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("mid", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 1500;
    msp.addMediaThread(atp);
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_3000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("mid", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 3000;
    msp.addMediaThread(atp);
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_500_1000_3000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 1500;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 3000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream8Kseg_1280x720_500_1000_3000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 8000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 1500;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 3000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_500_1000_3000_nosign)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 1500;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 3000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime,
                false);
}

TEST(BenchmarkLocalStream, VideoStream8kseg_1280x720_500_1000_3000_nosign)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 8000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 1280;
    atp.coderParams_.encodeHeight_ = 720;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 1500;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 3000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime,
                false);
}

TEST(BenchmarkLocalStream, VideoStream1280x720_700_3000)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 700;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 3000;
        msp.addMediaThread(atp);
    }
    
    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}
#endif
TEST(BenchmarkLocalStream, VideoStream1280x720_1200_500_100)
{
    MediaStreamParams msp("camera");
    
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshnessMs_ = 2000;
    msp.producerParams_.segmentSize_ = 1000;
    
    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    
    {
        VideoThreadParams atp("hi", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 1280;
        atp.coderParams_.encodeHeight_ = 720;
        atp.coderParams_.startBitrate_ = 1200;
        atp.coderParams_.maxBitrate_ = 1200;
        msp.addMediaThread(atp);
    }
    
    {
        VideoThreadParams atp("mid", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 640;
        atp.coderParams_.encodeHeight_ = 360;
        atp.coderParams_.startBitrate_ = 300;
        atp.coderParams_.maxBitrate_ = 300;
        msp.addMediaThread(atp);
    }

    {
        VideoThreadParams atp("low", sampleVideoCoderParams());
        atp.coderParams_.encodeWidth_ = 320;
        atp.coderParams_.encodeHeight_ = 180;
        atp.coderParams_.startBitrate_ = 100;
        atp.coderParams_.maxBitrate_ = 100;
        msp.addMediaThread(atp);
    }    

    runProducer(test_path+"/../res/test-source-1280x720.argb",
                boost::make_shared<ArgbFrame>(1280,720),
                msp,
                runtime);
}

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
