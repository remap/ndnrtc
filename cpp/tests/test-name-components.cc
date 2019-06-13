//
// test-name-components.cc
//
//  Created by Peter Gusev on 22 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/regex.hpp>

#include "gtest/gtest.h"
#include "include/name-components.hpp"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

#if 1
TEST(TestNameComponents, TestNameFiltering)
{
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_TRUE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_FALSE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_TRUE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_TRUE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/k/%FE%07/_parity/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_FALSE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_TRUE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/k/%FE%07/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_FALSE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_FALSE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_EQ(5, info.metaVersion_);
		EXPECT_EQ(SegmentClass::Meta, info.segmentClass_);
		EXPECT_TRUE(info.isMeta_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("", info.threadName_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_EQ(5, info.metaVersion_);
		EXPECT_EQ(SegmentClass::Meta, info.segmentClass_);
		EXPECT_TRUE(info.isMeta_);
	}
	{
		NamespaceInfo info;
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d", info));
		EXPECT_FALSE(info.hasSeqNo_);
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/k", info));
		EXPECT_FALSE(info.hasSeqNo_);
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FD%07/%00%00", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%03", info));
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%07/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, info.streamType_);
		EXPECT_EQ("mic", info.streamName_);
		EXPECT_TRUE(info.isMeta_);
		EXPECT_EQ("", info.threadName_);
		EXPECT_EQ(7, info.metaVersion_);
		EXPECT_EQ(SegmentClass::Meta, info.segmentClass_);
		EXPECT_EQ(0, info.segNo_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%03", info));
		EXPECT_TRUE(info.hasSeqNo_);
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, info.streamType_);
		EXPECT_EQ("mic", info.streamName_);
		EXPECT_EQ("hd", info.threadName_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(3, info.segNo_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, info.streamType_);
		EXPECT_EQ("mic", info.streamName_);
		EXPECT_EQ("hd", info.threadName_);
		EXPECT_TRUE(info.isMeta_);
		EXPECT_EQ(3, info.metaVersion_);
		EXPECT_EQ(SegmentClass::Meta, info.segmentClass_);
		EXPECT_EQ(0, info.segNo_);
	}
	{
		NamespaceInfo info;
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd", info));
		EXPECT_FALSE(info.hasSeqNo_);
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FD%03", info));
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03", info));
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/k/%FE%07/_manifest", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(SegmentClass::Manifest, info.segmentClass_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_FALSE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_FALSE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_manifest", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(SegmentClass::Manifest, info.segmentClass_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_TRUE(info.isDelta_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_FALSE(info.isParity_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/_manifest", info));
		EXPECT_TRUE(info.hasSeqNo_);
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, info.streamType_);
		EXPECT_EQ("mic", info.streamName_);
		EXPECT_EQ("hd", info.threadName_);
		EXPECT_FALSE(info.isMeta_);
		EXPECT_EQ(7, info.sampleNo_);
		EXPECT_EQ(SegmentClass::Manifest, info.segmentClass_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/icear/mobileterminal0/ndnrtc/%FD%02/video/back_camera/%FC%00%00%01c_%27%DE%D6/720p/_meta/%FD%00/%00%00", info));
		EXPECT_FALSE(info.hasSeqNo_);
		EXPECT_TRUE(info.hasSegNo_);
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ("/icear/mobileterminal0", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("back_camera", info.streamName_);
		EXPECT_EQ("720p", info.threadName_);
		EXPECT_TRUE(info.isMeta_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/icear/mobileterminal0/ndnrtc/%FD%02/video/back_camera/%FC%00%00%01c_%27%DE%D6/720p/_meta/%FD%00", info));
		EXPECT_FALSE(info.hasSeqNo_);
		EXPECT_FALSE(info.hasSegNo_);
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ("/icear/mobileterminal0", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("back_camera", info.streamName_);
		EXPECT_EQ("720p", info.threadName_);
		EXPECT_TRUE(info.isMeta_);
	}
    {
        NamespaceInfo info;
        EXPECT_TRUE(NameComponents::extractInfo("/ndn/user/rtc/ndnrtc/%FD%03/video/camera/%FC%00%00%01fU%98%BBA/1080p", info));
    }
    {
        NamespaceInfo info;
        EXPECT_TRUE(NameComponents::extractInfo("/touchdesigner/ndnrtcOut0/ndnrtc/%FD%03/video/video1/%FC%00%00%01f%9FK%08%B6", info));
        EXPECT_EQ(info.streamName_, "video1");
    }
}
#endif
#if 1
TEST(TestNameComponents, TestPrefixFiltering)
{
	using namespace prefix_filter;
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d/%FEG/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA"), info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d/%FEG/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d/%FEG/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d/%FEG"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(info.basePrefix_, info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(info.basePrefix_, info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd"), info.getPrefix(ThreadNT));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(info.basePrefix_, info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi"), info.getPrefix(ThreadNT));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00", info));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1"), info.getPrefix(0));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00"), info.getPrefix());
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00"), info.getPrefix(Segment));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta"), info.getPrefix(Sample));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(Thread));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(ThreadNT));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(StreamTS));
        EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera"), info.getPrefix(Stream));
		EXPECT_EQ(Name("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02") , info.getPrefix(Library));
	}
}
#endif
TEST(TestNameCompo, TestNameExtraction)
{
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/icear/user/mt1/ndnrtc/%FD%03/video/back_camera/%FC%00%00%01kG%A2%FB%D4/t/_meta", info));
	}
}
#if 0
TEST(TestNameComponents, TestSuffixFiltering)
{
	using namespace suffix_filter;
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FE%07/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hi/d/%FE%07/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/"), info.getSuffix());
		EXPECT_EQ(Name("/"), info.getSuffix(Segment));
		EXPECT_FALSE(info.hasSegNo_);
		EXPECT_EQ(Name("/%FE%07"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hi/d/%FE%07"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/_parity/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/_parity/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FE%07/_parity/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hi/d/%FE%07/_parity/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/_parity/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/"), info.getSuffix());
		EXPECT_EQ(Name("/"), info.getSuffix(Segment));
		EXPECT_FALSE(info.hasSegNo_);
		EXPECT_EQ(Name("/%FE%07"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hd/%FE%07"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FE%07/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hd/%FE%07/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/%FE%07/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FD%03/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hd/_meta/%FD%03/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/audio/mic/%FC%00%00%01c_%27%DE%D6/hd/_meta/%FD%03/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FD%03/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/%FD%03/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/audio/mic/_meta/%FD%03/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/audio/mic/_meta/%FD%03/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FD%05/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/hi/_meta/%FD%05/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00") , info.getSuffix(Library));
	}
	{
		NamespaceInfo info;
		ASSERT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00", info));
		EXPECT_EQ(Name(), info.getSuffix(0));
		EXPECT_EQ(Name("/%00%00"), info.getSuffix());
		EXPECT_EQ(Name("/%00%00"), info.getSuffix(Segment));
		EXPECT_EQ(Name("/%FD%05/%00%00"), info.getSuffix(Sample));
		EXPECT_EQ(Name("/%FD%05/%00%00"), info.getSuffix(Thread));
		EXPECT_EQ(Name("/video/camera/_meta/%FD%05/%00%00"), info.getSuffix(Stream));
		EXPECT_EQ(Name("/ndnrtc/%FD%02/video/camera/_meta/%FD%05/%00%00") , info.getSuffix(Library));
	}
}
#endif
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
