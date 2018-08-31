//
// test-client-params.cc
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "config.hpp"
#include "include/params.hpp"
#include "tests-helpers.hpp"

using namespace std;
using namespace ndnrtc;

TEST(TestStatGatheringParams, TestOutput)
{
    StatGatheringParams sgp("buffer");
    stringstream ss;

    sgp.addStat("jitterPlay");
    sgp.addStat("jitterTar");
    sgp.addStat("drdPrime");

    ss << sgp;
    EXPECT_EQ("stat file: buffer.stat; stats: (jitterPlay, jitterTar, drdPrime)", ss.str());
}

TEST(TestStatGatheringParams, TestOutput2)
{
    StatGatheringParams sgp("buffer");
    stringstream ss;

    static const char *s[] = {"jitterPlay", "jitterTar", "drdPrime"};
    vector<string> v(s, s + 3);
    sgp.addStats(v);

    ss << sgp;
    EXPECT_EQ("stat file: buffer.stat; stats: (jitterPlay, jitterTar, drdPrime)", ss.str());
}

TEST(TestClientMediaStreamParams, TestOutput)
{
    ClientMediaStreamParams msp;
    stringstream ss;

    msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp.streamName_ = "mic";
    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.synchronizedStreamName_ = "camera";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp.addMediaThread(AudioThreadParams("g722"));

    ss << msp;
    EXPECT_EQ("session prefix: /ndn/edu/ucla/remap/ndnrtc/user/client1; "
              "name: mic (audio); synced to: camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n",
              ss.str());
}

TEST(TestProducerStreamParams, TestOutputAndCopy)
{
    ProducerStreamParams msp;
    stringstream ss;

    msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp.source_ = "mic.pcmu";
    msp.streamName_ = "mic";
    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.synchronizedStreamName_ = "camera";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp.addMediaThread(AudioThreadParams("g722"));

    ProducerStreamParams mspCopy(msp);

    ss << mspCopy;
    EXPECT_EQ("stream source: mic.pcmu; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); synced to:"
              " camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n",
              ss.str());
}

TEST(TestConsumerStreamParams, TestOutput)
{
    ConsumerStreamParams msp;
    stringstream ss;

    msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp.sink_.name_ = "mic.pcmu";
    msp.threadToFetch_ = "pcmu";
    msp.streamName_ = "mic";
    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.synchronizedStreamName_ = "camera";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp.addMediaThread(AudioThreadParams("g722"));

    ss << msp;
    EXPECT_EQ("stream sink: mic.pcmu (type: file); thread to fetch: pcmu; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); synced to:"
              " camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n",
              ss.str());
}

TEST(TestProducerStreamParams, TestVideoProducerParams)
{
    ProducerStreamParams msp;

    msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp.source_ = "camera.argb";
    msp.streamName_ = "mic";
    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;

    {
        VideoThreadParams vp("low", sampleVideoCoderParams());
        vp.coderParams_.encodeWidth_ = 320;
        vp.coderParams_.encodeHeight_ = 280;

        msp.addMediaThread(vp);
    }

    {
        VideoThreadParams vp("low", sampleVideoCoderParams());
        vp.coderParams_.encodeWidth_ = 640;
        vp.coderParams_.encodeHeight_ = 480;

        msp.addMediaThread(vp);
    }

    unsigned int height = 0, width = 0;
    msp.getMaxResolution(width, height);
    EXPECT_EQ(640, width);
    EXPECT_EQ(480, height);
}

TEST(TestProducerStreamParams, TestAudioProducerParams)
{
    ProducerStreamParams msp;

    msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp.source_ = "camera.argb";
    msp.streamName_ = "mic";
    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp.addMediaThread(AudioThreadParams("g722"));

    unsigned int height = 0, width = 0;
    msp.getMaxResolution(width, height);
    EXPECT_EQ(0, width);
    EXPECT_EQ(0, height);
}

TEST(TestConsumerClientParams, TestOutputEmpty)
{
    ConsumerClientParams ccp;

    stringstream ss;
    ss << ccp;

    EXPECT_EQ("general audio: interest lifetime: 0 ms; jitter size: 0 ms\n"
              "general video: interest lifetime: 0 ms; jitter size: 0 ms\n",
              ss.str());
}

TEST(TestConsumerClientParams, TestOutput)
{
    ConsumerStreamParams msp1;

    msp1.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp1.sink_.name_ = "mic.pcmu";
    msp1.threadToFetch_ = "pcmu";
    msp1.streamName_ = "mic";
    msp1.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp1.synchronizedStreamName_ = "camera";
    msp1.producerParams_.freshness_ = {10, 15, 900};
    msp1.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp1.captureDevice_ = cdp;
    msp1.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp1.addMediaThread(AudioThreadParams("g722"));

    ConsumerStreamParams msp2;

    msp2.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp2.sink_.name_ = "camera.yuv";
    msp2.threadToFetch_ = "low";
    msp2.streamName_ = "camera";
    msp2.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp2.synchronizedStreamName_ = "mic";
    msp2.producerParams_.freshness_ = {10, 15, 900};
    msp2.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp2;
    cdp2.deviceId_ = 11;
    msp2.captureDevice_ = cdp2;
    msp2.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
    msp2.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

    GeneralConsumerParams gcpa;

    gcpa.interestLifetime_ = 2000;
    gcpa.jitterSizeMs_ = 150;

    GeneralConsumerParams gcpv;

    gcpv.interestLifetime_ = 2000;
    gcpv.jitterSizeMs_ = 150;

    StatGatheringParams sgp("buffer");
    static const char *s[] = {"jitterPlay", "jitterTar", "drdPrime"};
    vector<string> v(s, s + 3);
    sgp.addStats(v);

    ConsumerClientParams ccp;
    ccp.generalAudioParams_ = gcpa;
    ccp.generalVideoParams_ = gcpv;
    ccp.statGatheringParams_.push_back(sgp);

    ccp.fetchedStreams_.push_back(msp1);
    ccp.fetchedStreams_.push_back(msp2);

    stringstream ss;
    ss << ccp;

    EXPECT_EQ("general audio: interest lifetime: 2000 ms; jitter size: 150 ms"
              "\ngeneral video: "
              "interest lifetime: 2000 ms; jitter size: 150 ms"
              "\nstat gathering:\n"
              "stat file: buffer.stat; stats: (jitterPlay, jitterTar, drdPrime)\n"
              "fetching:\n"
              "[0: stream sink: mic.pcmu (type: file); thread to fetch: pcmu; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); synced to:"
              " camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n]\n"
              "[1: stream sink: camera.yuv (type: file); thread to fetch: low; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: camera (video); "
              "synced to: mic; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 11; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: hi; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n]\n",
              ss.str());
}

TEST(TestConsumerClientParams, TestCopy)
{
    ConsumerStreamParams msp1;

    msp1.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp1.sink_.name_ = "mic.pcmu";
    msp1.threadToFetch_ = "pcmu";
    msp1.streamName_ = "mic";
    msp1.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp1.synchronizedStreamName_ = "camera";
    msp1.producerParams_.freshness_ = {10, 15, 900};
    msp1.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp1.captureDevice_ = cdp;
    msp1.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
    msp1.addMediaThread(AudioThreadParams("g722"));

    ConsumerStreamParams msp2;

    msp2.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
    msp2.sink_.name_ = "camera.yuv";
    msp2.threadToFetch_ = "low";
    msp2.streamName_ = "camera";
    msp2.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp2.synchronizedStreamName_ = "mic";
    msp2.producerParams_.freshness_ = {10, 15, 900};
    msp2.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp2;
    cdp2.deviceId_ = 11;
    msp2.captureDevice_ = cdp2;
    msp2.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
    msp2.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

    GeneralConsumerParams gcpa;

    gcpa.interestLifetime_ = 2000;
    gcpa.jitterSizeMs_ = 150;

    GeneralConsumerParams gcpv;

    gcpv.interestLifetime_ = 2000;
    gcpv.jitterSizeMs_ = 150;

    StatGatheringParams sgp("buffer");
    static const char *s[] = {"jitterPlay", "jitterTar", "drdPrime"};
    vector<string> v(s, s + 3);
    sgp.addStats(v);

    ConsumerClientParams ccp;
    ccp.generalAudioParams_ = gcpa;
    ccp.generalVideoParams_ = gcpv;
    ccp.statGatheringParams_.push_back(sgp);

    ccp.fetchedStreams_.push_back(msp1);
    ccp.fetchedStreams_.push_back(msp2);

    ConsumerClientParams ccpCopy(ccp);

    ccp.fetchedStreams_[0].sink_.name_ = "another.sink";
    ccp.fetchedStreams_[0].threadToFetch_ = "another-thread";
    ccp.statGatheringParams_.clear();
    ccp.generalAudioParams_ = GeneralConsumerParams();
    ccp.generalVideoParams_ = GeneralConsumerParams();

    stringstream ss;
    ss << ccpCopy;

    EXPECT_EQ("general audio: interest lifetime: 2000 ms; jitter size: 150 ms"
              "\ngeneral video: "
              "interest lifetime: 2000 ms; jitter size: 150 ms"
              "\nstat gathering:\n"
              "stat file: buffer.stat; stats: (jitterPlay, jitterTar, drdPrime)\n"
              "fetching:\n"
              "[0: stream sink: mic.pcmu (type: file); thread to fetch: pcmu; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); synced to:"
              " camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n]\n"
              "[1: stream sink: camera.yuv (type: file); thread to fetch: low; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: camera (video); "
              "synced to: mic; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 11; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: hi; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n]\n",
              ss.str());
}

TEST(TestClientParams, TestOutputEmpty)
{
    ClientParams cp;
    stringstream ss;

    EXPECT_EQ("", cp.getGeneralParameters().logFile_);
    EXPECT_EQ(ndnlog::NdnLoggerDetailLevelNone, cp.getGeneralParameters().loggingLevel_);
    EXPECT_EQ("", cp.getGeneralParameters().logPath_);
    EXPECT_FALSE(cp.getGeneralParameters().useFec_);
    EXPECT_FALSE(cp.getGeneralParameters().useAvSync_);
    EXPECT_EQ("", cp.getGeneralParameters().host_);
    EXPECT_EQ(6363, cp.getGeneralParameters().portNum_);

    ss << cp;
    EXPECT_EQ("-general:\nlog level: NONE; log file:  (at ); FEC: OFF; "
              "A/V Sync: OFF; Host: ; Port #: 6363\n",
              ss.str());
}

TEST(TestClientParams, TestOutput)
{
    ClientParams cp;
    stringstream ss;

    {
        GeneralParams gp;

        gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
        gp.logFile_ = "ndnrtc.log";
        gp.logPath_ = "/tmp";
        gp.useFec_ = false;
        gp.useAvSync_ = true;
        gp.host_ = "aleph.ndn.ucla.edu";
        gp.portNum_ = 6363;
        cp.setGeneralParameters(gp);
    }

    {
        ProducerClientParams pcp;

        pcp.prefix_ = "/ndn/edu/ucla/remap";

        ProducerStreamParams msp;
        stringstream ss;

        msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
        msp.streamName_ = "mic";
        msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
        msp.producerParams_.freshness_ = {10, 15, 900};
        msp.producerParams_.segmentSize_ = 1000;

        CaptureDeviceParams cdp;
        cdp.deviceId_ = 10;
        msp.captureDevice_ = cdp;
        msp.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
        msp.addMediaThread(AudioThreadParams("g722"));

        pcp.publishedStreams_.push_back(msp);

        {
            ProducerStreamParams msp;
            stringstream ss;

            msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
            msp.streamName_ = "camera";
            msp.source_ = "camera.yuv";
            msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
            msp.synchronizedStreamName_ = "mic";
            msp.producerParams_.freshness_ = {10, 15, 900};
            msp.producerParams_.segmentSize_ = 1000;

            CaptureDeviceParams cdp;
            cdp.deviceId_ = 11;
            msp.captureDevice_ = cdp;
            msp.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
            msp.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

            pcp.publishedStreams_.push_back(msp);
        }

        EXPECT_FALSE(cp.isProducing());
        cp.setProducerParams(pcp);
        EXPECT_TRUE(cp.isProducing());
        EXPECT_EQ(2, cp.getProducerParams().publishedStreams_.size());
    }

    {
        ConsumerStreamParams msp1;

        msp1.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
        msp1.sink_.name_ = "mic.pcmu";
        msp1.threadToFetch_ = "pcmu";
        msp1.streamName_ = "mic";
        msp1.type_ = MediaStreamParams::MediaStreamTypeAudio;
        msp1.synchronizedStreamName_ = "camera";
        msp1.producerParams_.freshness_ = {10, 15, 900};
        msp1.producerParams_.segmentSize_ = 1000;

        CaptureDeviceParams cdp;
        cdp.deviceId_ = 10;
        msp1.captureDevice_ = cdp;
        msp1.addMediaThread(AudioThreadParams("pcmu", "pcmu"));
        msp1.addMediaThread(AudioThreadParams("g722"));

        ConsumerStreamParams msp2;

        msp2.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
        msp2.sink_.name_ = "camera.yuv";
        msp2.threadToFetch_ = "low";
        msp2.streamName_ = "camera";
        msp2.type_ = MediaStreamParams::MediaStreamTypeVideo;
        msp2.synchronizedStreamName_ = "mic";
        msp2.producerParams_.freshness_ = {10, 15, 900};
        msp2.producerParams_.segmentSize_ = 1000;

        CaptureDeviceParams cdp2;
        cdp2.deviceId_ = 11;
        msp2.captureDevice_ = cdp2;
        msp2.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
        msp2.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

        GeneralConsumerParams gcpa;

        gcpa.interestLifetime_ = 2000;
        gcpa.jitterSizeMs_ = 150;

        GeneralConsumerParams gcpv;

        gcpv.interestLifetime_ = 2000;
        gcpv.jitterSizeMs_ = 150;

        StatGatheringParams sgp("buffer");
        static const char *s[] = {"jitterPlay", "jitterTar", "drdPrime"};
        vector<string> v(s, s + 3);
        sgp.addStats(v);

        ConsumerClientParams ccp;
        ccp.generalAudioParams_ = gcpa;
        ccp.generalVideoParams_ = gcpv;
        ccp.statGatheringParams_.push_back(sgp);

        ccp.fetchedStreams_.push_back(msp1);
        ccp.fetchedStreams_.push_back(msp2);

        EXPECT_FALSE(cp.isConsuming());
        cp.setConsumerParams(ccp);
        EXPECT_TRUE(cp.isConsuming());
        EXPECT_EQ(2, cp.getConsumedStreamsNum());
    }

    ss << cp;
    EXPECT_EQ("-general:\nlog level: TRACE; log file: ndnrtc.log (at /tmp); FEC: OFF; "
              "A/V Sync: ON; Host: aleph.ndn.ucla.edu; Port #: 6363\n"
              "-consuming:\n"
              "general audio: interest lifetime: 2000 ms; jitter size: 150 ms"
              "\ngeneral video: "
              "interest lifetime: 2000 ms; jitter size: 150 ms"
              "\nstat gathering:\n"
              "stat file: buffer.stat; stats: (jitterPlay, jitterTar, drdPrime)\n"
              "fetching:\n"
              "[0: stream sink: mic.pcmu (type: file); thread to fetch: pcmu; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); synced to:"
              " camera; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n]\n"
              "[1: stream sink: camera.yuv (type: file); thread to fetch: low; session prefix: "
              "/ndn/edu/ucla/remap/ndnrtc/user/client1; name: camera (video); "
              "synced to: mic; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 11; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: hi; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n]\n"
              "-producing:\nprefix: /ndn/edu/ucla/remap;\n"
              "--0:\n"
              "stream source: ; session prefix: /ndn/edu/ucla/remap/ndnrtc/user/client1; name: mic (audio); "
              "synced to: ; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 10; 2 threads:\n"
              "[0: name: pcmu; codec: pcmu]\n[1: name: g722; codec: g722]\n"
              "--1:\n"
              "stream source: camera.yuv; session prefix: /ndn/edu/ucla/remap/ndnrtc/user/client1; name: camera (video); "
              "synced to: mic; seg size: 1000 bytes; freshness (ms): metadata 10 sample 15 sample (key) 900; "
              "capture device id: 11; 2 threads:\n"
              "[0: name: low; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n"
              "[1: name: hi; 30FPS; GOP: 30; Start bitrate: 1000 Kbit/s; "
              "Max bitrate: 3000 Kbit/s; 1920x1080; Drop: YES]\n",
              ss.str());
}

//******************************************************************************
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
