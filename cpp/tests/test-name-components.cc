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

TEST(TestNameComponents, TestStaticMethods)
{
	EXPECT_EQ(NameComponents::fullVersion(), "v4.0.0");
	EXPECT_EQ(NameComponents::nameApiVersion(), 4);
	EXPECT_EQ(NameComponents::ndnrtcSuffix(), Name("ndnrtc/%FD%04"));

	Name streamPrefix = NameComponents::streamPrefix("/my/base/prefix", "mystream");
	EXPECT_EQ(streamPrefix[-1].toEscapedString(), "mystream");
	EXPECT_EQ(streamPrefix.getPrefix(-4).toUri(), "/my/base/prefix");
	EXPECT_EQ(streamPrefix.getSubName(-4, 2), Name("ndnrtc/%FD%04"));
	EXPECT_TRUE(streamPrefix[-2].isTimestamp());
}

TEST(TestNameComponents, TestNamespaceInfoExtraction)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(0).append(streamName);
	EXPECT_TRUE(NameComponents::extractInfo(n1, ninfo));

	ninfo.reset();
	// stream meta
	Name n2 = Name(n1).append("_meta");
	// stream live meta
	Name n3 = Name(n1).append("_live").appendVersion(0);
	// stream latest pointer
	Name n4 = Name(n1).append("_latest").appendVersion(0);
	// stream gop
	Name n5 = Name(n1).append("_gop").appendSequenceNumber(0).append("start");
	Name n6 = Name(n1).append("_gop").appendSequenceNumber(0).append("end");
	// frame
	Name n7 = Name(n1).appendSequenceNumber(0);
	// frame meta
	Name n8 = Name(n7).append("_meta");
	// frame manifest
	Name n9 = Name(n7).append("_manifest");
	// frame data
	Name n10 = Name(n7).appendSegment(0);
	// frame parity
	Name n11 = Name(n7).append("_parity").appendSegment(0);
}

#if 0
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
		EXPECT_EQ(5, info.segmentVersion_);
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
		EXPECT_EQ(5, info.segmentVersion_);
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
		EXPECT_EQ(7, info.segmentVersion_);
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
		EXPECT_EQ(3, info.segmentVersion_);
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
    {
        NamespaceInfo info;
        EXPECT_TRUE(NameComponents::extractInfo("/ndnrtc-loopback/producer/ndnrtc/%FD%03/video/camera/%FC%00%00%01g%9A%0F%86%0C/320p/_latest/%FD%05%87", info));
        EXPECT_EQ(info.segmentClass_, SegmentClass::Pointer);
        EXPECT_NE(info.segmentVersion_, 0);
    }
    {
        NamespaceInfo info;
        EXPECT_TRUE(NameComponents::extractInfo("/ndnrtc-loopback/producer/ndnrtc/%FD%03/audio/mic/%FC%00%00%01g%9A%0F%86%0C/pcmu/_latest/%FD%05%87", info));
        EXPECT_EQ(info.segmentClass_, SegmentClass::Pointer);
        EXPECT_NE(info.segmentVersion_, 0);
    }
}
#endif
#if 0
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
