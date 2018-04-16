//
// test-params.cc
//
//  Created by Peter Gusev on 03 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "include/params.hpp"

#define PARAMS
#include "tests-helpers.hpp"

using namespace std;
using namespace ndnrtc;

TEST(TestBase, TestOutput)
{
    Params p;
    stringstream ss;

    ss << p;

    EXPECT_EQ(ss.str(), "");
}

TEST(TestCaptureDeviceParams, TestCtor)
{
    CaptureDeviceParams cdp;
    EXPECT_LT(cdp.deviceId_, 0);
}

TEST(TestCaptureDeviceParams, TestOutput)
{
    CaptureDeviceParams cdp;
    stringstream ss;

    cdp.deviceId_ = 10;
    ss << cdp;

    EXPECT_EQ(10, cdp.deviceId_);
    EXPECT_EQ(ss.str(), "id: 10");
}

TEST(TestAudioCaptureDeviceParams, TestOutput)
{
    AudioCaptureParams acp;
    stringstream ss;

    acp.deviceId_ = 10;
    ss << acp;

    EXPECT_EQ(10, acp.deviceId_);
    EXPECT_EQ(ss.str(), "id: 10");
}

TEST(TestFrameSegmentsInfo, TestCtor)
{
    FrameSegmentsInfo fsi;

    EXPECT_EQ(0, fsi.deltaAvgParitySegNum_);
    EXPECT_EQ(0, fsi.deltaAvgSegNum_);
    EXPECT_EQ(0, fsi.keyAvgSegNum_);
    EXPECT_EQ(0, fsi.keyAvgParitySegNum_);
}

TEST(TestFrameSegmentsInfo, TestCtor2)
{
    FrameSegmentsInfo fsi(1, 2, 3, 4);

    EXPECT_EQ(1, fsi.deltaAvgSegNum_);
    EXPECT_EQ(2, fsi.deltaAvgParitySegNum_);
    EXPECT_EQ(3, fsi.keyAvgSegNum_);
    EXPECT_EQ(4, fsi.keyAvgParitySegNum_);
}

TEST(TestFrameSegmentsInfo, TestComparison)
{
    FrameSegmentsInfo fsi(1, 2, 3, 4);
    FrameSegmentsInfo fsi2(1, 2, 3, 4);
    FrameSegmentsInfo fsi3(1, 2, 3, 0);
    FrameSegmentsInfo fsi4(1, 2, 0, 4);
    FrameSegmentsInfo fsi5(1, 0, 3, 4);
    FrameSegmentsInfo fsi6(0, 2, 3, 4);

    EXPECT_TRUE(fsi == fsi2);
    EXPECT_TRUE(fsi != fsi3);
    EXPECT_TRUE(fsi != fsi4);
    EXPECT_TRUE(fsi != fsi5);
    EXPECT_TRUE(fsi != fsi6);
}

TEST(TestAudioThreadParams, TestFields)
{
    AudioThreadParams mtp;

    mtp.threadName_ = "test-thread";
    mtp.segInfo_ = FrameSegmentsInfo(1, 2, 3, 4);

    EXPECT_EQ("test-thread", mtp.threadName_);
    EXPECT_EQ(FrameSegmentsInfo(1, 2, 3, 4), mtp.segInfo_);
}

TEST(TestAudioThreadParams, TestOutput)
{
    AudioThreadParams mtp;
    stringstream ss;

    mtp.threadName_ = "test-thread";
    ss << mtp;

    EXPECT_EQ(ss.str(), "name: test-thread; codec: g722");
    mtp.codec_ = "opus";

    ss.str("");
    ss << mtp;
    EXPECT_EQ(ss.str(), "name: test-thread; codec: opus");
}

TEST(TestAudioThreadParams, TestAccessors)
{
    AudioThreadParams atp;

    EXPECT_EQ(FrameSegmentsInfo(1., 0., 0., 0.), atp.getSegmentsInfo());
}

TEST(TestAudioThreadParams, TestCtor)
{
    AudioThreadParams atp("audio-thread");

    EXPECT_EQ("audio-thread", atp.threadName_);
    EXPECT_EQ(FrameSegmentsInfo(1., 0., 0., 0.), atp.getSegmentsInfo());
}

TEST(TestAudioThreadParams, TestCopy)
{
    AudioThreadParams atp, *atpCopy;
    stringstream ss;

    atp.threadName_ = "audio-thread-original";
    atpCopy = (AudioThreadParams *)atp.copy();
    ss << *atpCopy;

    EXPECT_EQ("audio-thread-original", atpCopy->threadName_);
    EXPECT_EQ(ss.str(), "name: audio-thread-original; codec: g722");
    EXPECT_EQ(FrameSegmentsInfo(1., 0., 0., 0.), atpCopy->getSegmentsInfo());

    atp.threadName_ = "audio-thread-changed";
    EXPECT_EQ("audio-thread-original", atpCopy->threadName_);

    delete atpCopy;
}

TEST(TestVideoCoderParams, TestOutput)
{
    VideoCoderParams vcp(sampleVideoCoderParams());
    stringstream ss;

    ss << vcp;

    EXPECT_EQ(ss.str(),
              "30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES");
}

TEST(TestVideoCoderParams, TestOutput2)
{
    VideoCoderParams vcp;
    stringstream ss;

    ss << vcp;

    EXPECT_EQ(ss.str(),
              "0FPS; GOP: 0; Start bitrate: 0 Kbit/s; "
              "Max bitrate: 0 Kbit/s; 0x0; Drop: NO");
}

TEST(TestVideoCoderParams, TestOutput3)
{
    VideoCoderParams vcp(sampleVideoCoderParams());
    stringstream ss;

    vcp.dropFramesOn_ = false;
    ss << vcp;

    EXPECT_EQ(ss.str(),
              "30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: NO");
}

TEST(TestVideoCoderParams, TestCopyCtor)
{
    VideoCoderParams vcp(sampleVideoCoderParams());
    stringstream ss;
    VideoCoderParams vcp2(vcp);

    EXPECT_EQ(vcp, vcp2);

    ss << vcp2;
    EXPECT_EQ(ss.str(),
              "30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES");
}

TEST(TestVideoThreadParams, TestCtor)
{
    VideoThreadParams vtp("video-thread");

    EXPECT_EQ(FrameSegmentsInfo(), vtp.getSegmentsInfo());
    EXPECT_EQ(VideoCoderParams(), vtp.coderParams_);
    EXPECT_EQ("video-thread", vtp.threadName_);
}

TEST(TestVideoThreadParams, TestCtor2)
{
    VideoThreadParams vtp("video-thread", FrameSegmentsInfo(1, 2, 3, 4));

    EXPECT_EQ(FrameSegmentsInfo(1, 2, 3, 4), vtp.getSegmentsInfo());
    EXPECT_EQ(VideoCoderParams(), vtp.coderParams_);
    EXPECT_EQ("video-thread", vtp.threadName_);
}

TEST(TestVideoThreadParams, TestCtor3)
{
    VideoThreadParams vtp("video-thread", sampleVideoCoderParams());

    EXPECT_EQ(FrameSegmentsInfo(), vtp.getSegmentsInfo());
    EXPECT_EQ(sampleVideoCoderParams(), vtp.coderParams_);
    EXPECT_EQ("video-thread", vtp.threadName_);
}

TEST(TestVideoThreadParams, TestCtor4)
{
    VideoThreadParams vtp("video-thread", FrameSegmentsInfo(1, 2, 3, 4), sampleVideoCoderParams());

    EXPECT_EQ(FrameSegmentsInfo(1, 2, 3, 4), vtp.getSegmentsInfo());
    EXPECT_EQ(sampleVideoCoderParams(), vtp.coderParams_);
    EXPECT_EQ("video-thread", vtp.threadName_);
}

TEST(TestVideoThreadParams, TestOutput)
{
    VideoThreadParams vtp;
    stringstream ss;

    ss << vtp;

    EXPECT_EQ(ss.str(),
              "name: ; 0FPS; GOP: 0; Start bitrate: 0 Kbit/s; "
              "Max bitrate: 0 Kbit/s; 0x0; Drop: NO");
}

TEST(TestVideoThreadParams, TestOutput2)
{
    VideoThreadParams vtp;
    stringstream ss;

    vtp.coderParams_ = sampleVideoCoderParams();
    ss << vtp;

    EXPECT_EQ(ss.str(),
              "name: ; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES");
}

TEST(TestVideoThreadParams, TestCopy)
{
    VideoThreadParams vtp("thread-original", FrameSegmentsInfo(1, 2, 3, 4), sampleVideoCoderParams()),
        *vtpCopy = (VideoThreadParams *)vtp.copy();
    stringstream ss;

    vtp.threadName_ = "thread-changed";
    vtp.segInfo_ = FrameSegmentsInfo();
    vtp.coderParams_ = VideoCoderParams();

    ss << *vtpCopy;
    EXPECT_EQ(ss.str(),
              "name: thread-original; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES");
    EXPECT_EQ(FrameSegmentsInfo(1, 2, 3, 4), vtpCopy->getSegmentsInfo());
    EXPECT_EQ(sampleVideoCoderParams(), vtpCopy->coderParams_);
}

TEST(TestGeneralProducerParams, TestOutput)
{
    GeneralProducerParams gpp;
    stringstream ss;

    ss << gpp;
    EXPECT_EQ(ss.str(), 
              "seg size: 0 bytes; freshness (ms): metadata 0 sample 0 sample (key) 0");
}

TEST(TestGeneralProducerParams, TestOutput2)
{
    GeneralProducerParams gpp;
    stringstream ss;

    gpp.segmentSize_ = 1000;
    gpp.freshness_ = {10, 15, 900};

    ss << gpp;
    EXPECT_EQ(ss.str(), 
              "seg size: 1000 bytes; freshness (ms): "
              "metadata 10 sample 15 sample (key) 900");
}

TEST(TestMediaStreamParams, TestAccessors)
{
    MediaStreamParams msp("mic");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.synchronizedStreamName_ = "camera";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(AudioThreadParams("pcmu"));
    msp.addMediaThread(AudioThreadParams("g722"));

    EXPECT_EQ(2, msp.getThreadNum());
    EXPECT_EQ("pcmu", msp.getMediaThread(0)->threadName_);
    EXPECT_EQ("g722", msp.getMediaThread(1)->threadName_);
    EXPECT_EQ("pcmu", msp.getAudioThread(0)->threadName_);
    EXPECT_EQ("g722", msp.getAudioThread(1)->threadName_);
    EXPECT_FALSE(msp.getVideoThread(0));
    EXPECT_FALSE(msp.getVideoThread(1));
    EXPECT_FALSE(msp.getMediaThread(2));
    EXPECT_FALSE(msp.getMediaThread(-1));
}

TEST(TestMediaStreamParams, TestAccessors2)
{
    MediaStreamParams msp("camera");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
    msp.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

    EXPECT_EQ(2, msp.getThreadNum());
    EXPECT_EQ("low", msp.getMediaThread(0)->threadName_);
    EXPECT_EQ("hi", msp.getMediaThread(1)->threadName_);
    EXPECT_EQ("low", msp.getVideoThread(0)->threadName_);
    EXPECT_EQ("hi", msp.getVideoThread(1)->threadName_);
    EXPECT_FALSE(msp.getAudioThread(0));
    EXPECT_FALSE(msp.getAudioThread(1));
}

TEST(TestMediaStreamParams, TestAccessors3)
{
    MediaStreamParams msp("mic");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
    msp.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

    EXPECT_EQ(0, msp.getThreadNum());
    EXPECT_FALSE(msp.getMediaThread(0));
    EXPECT_FALSE(msp.getMediaThread(1));
}

TEST(TestMediaStreamParams, TestAccessors4)
{
    MediaStreamParams msp("cam");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.addMediaThread(AudioThreadParams("t1"));
    msp.addMediaThread(AudioThreadParams("t2"));

    EXPECT_EQ(0, msp.getThreadNum());
    EXPECT_FALSE(msp.getMediaThread(0));
    EXPECT_FALSE(msp.getMediaThread(1));
}

TEST(TestMediaStreamParams, TestOutput)
{
    MediaStreamParams msp;
    stringstream ss;

    ss << msp;
    EXPECT_EQ(ss.str(), 
              "name:  (audio); synced to: ; seg size: 0 bytes; freshness (ms): metadata 0 sample 0 sample (key) 0; "
              "no device; 0 threads:\n");
}

TEST(TestMediaStreamParams, TestOutput2)
{
    MediaStreamParams msp("mic");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.synchronizedStreamName_ = "camera";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;

    msp.addMediaThread(AudioThreadParams("sd"));
    msp.addMediaThread(AudioThreadParams("hd", "opus"));

    ss << msp;
    EXPECT_EQ(ss.str(),
              "name: mic (audio); synced to: camera; seg size: 1000 bytes; "
              "freshness (ms): metadata 10 sample 15 sample (key) 900; capture device id: 10; 2 threads:\n"
              "[0: name: sd; codec: g722]\n[1: name: hd; codec: opus]\n");
}

TEST(TestMediaStreamParams, TestOutput3)
{
    MediaStreamParams msp("camera");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;

    VideoThreadParams atp("low", sampleVideoCoderParams());
    msp.addMediaThread(atp);

    VideoThreadParams atp2("mid", sampleVideoCoderParams());
    msp.addMediaThread(atp2);

    ss << msp;
    EXPECT_EQ(ss.str(),
              "name: camera (video); synced to: mic; seg size: 1000 bytes; "
              "freshness (ms): metadata 10 sample 15 sample (key) 900; capture device id: 10; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: mid; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n");
}

TEST(TestMediaStreamParams, TestCopy)
{
    MediaStreamParams msp("camera");
    stringstream ss;

    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;

    VideoThreadParams atp("low", sampleVideoCoderParams());
    msp.addMediaThread(atp);

    VideoThreadParams atp2("mid", sampleVideoCoderParams());
    msp.addMediaThread(atp2);

    MediaStreamParams mspCopy(msp);
    ss << mspCopy;
    EXPECT_EQ(ss.str(),
              "name: camera (video); synced to: mic; seg size: 1000 bytes; "
              "freshness (ms): metadata 10 sample 15 sample (key) 900; capture device id: 10; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: mid; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n");
}

TEST(TestGeneralConsumerParams, TestOutput)
{
    GeneralConsumerParams gcp;

    gcp.interestLifetime_ = 2000;
    gcp.jitterSizeMs_ = 150;

    stringstream ss;

    ss << gcp;
    EXPECT_EQ(ss.str(), "interest lifetime: 2000 ms; jitter size: 150 ms");
}

TEST(TestGeneralParams, TestDefault)
{
    GeneralParams gp;

    EXPECT_EQ("", gp.logFile_);
    EXPECT_EQ(ndnlog::NdnLoggerDetailLevelNone, gp.loggingLevel_);
    EXPECT_EQ("", gp.logPath_);
    EXPECT_FALSE(gp.useFec_);
    EXPECT_FALSE(gp.useAvSync_);
    EXPECT_EQ("", gp.host_);
    EXPECT_EQ(6363, gp.portNum_);
}

TEST(TestGeneralParams, TestOutputEmpty)
{
    GeneralParams gp;
    stringstream ss;

    ss << gp;
    EXPECT_EQ(ss.str(),
              "log level: NONE; log file:  (at ); FEC: OFF; "
              "A/V Sync: OFF; Host: ; Port #: 6363");
}

TEST(TestGeneralParams, TestOutput)
{
    GeneralParams gp;

    gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
    gp.logFile_ = "ndnrtc.log";
    gp.logPath_ = "/tmp";
    gp.useFec_ = false;
    gp.useAvSync_ = true;
    gp.host_ = "aleph.ndn.ucla.edu";
    gp.portNum_ = 6363;

    stringstream ss;
    ss << gp;
    EXPECT_EQ(ss.str(),
              "log level: TRACE; log file: ndnrtc.log (at /tmp); FEC: OFF; "
              "A/V Sync: ON; Host: aleph.ndn.ucla.edu; Port #: 6363");
}

//******************************************************************************
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
