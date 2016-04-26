// 
// test-name-components.cc
//
//  Created by Peter Gusev on 22 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/regex.hpp>

#include "gtest/gtest.h"
#include "include/name-components.h"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

TEST(TestNameComponents, TestNameFiltering)
{
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00", info));
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
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/_parity/%00%00", info));
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
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/k/%FE%07/_parity/%00%00", info));
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
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/k/%FE%07/%00%00", info));
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
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/_meta/%FD%05/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, info.streamType_);
		EXPECT_EQ("camera", info.streamName_);
		EXPECT_EQ("hi", info.threadName_);
		EXPECT_EQ(0, info.segNo_);
		EXPECT_EQ(5, info.metaVersion_);
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
		EXPECT_TRUE(info.isMeta_);
	}
	{
		NamespaceInfo info;
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FD%07/%00%00", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/_meta/%FD%03", info));
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
		EXPECT_EQ(0, info.segNo_);
	}
	{
		NamespaceInfo info;
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/%FE%07/%00%03", info));
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
		EXPECT_TRUE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/_meta/%FD%03/%00%00", info));
		EXPECT_EQ("/ndn/edu/ucla/remap/peter/ndncon/instance1", info.basePrefix_.toUri());
		EXPECT_EQ(2, info.apiVersion_);
		EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, info.streamType_);
		EXPECT_EQ("mic", info.streamName_);
		EXPECT_EQ("hd", info.threadName_);
		EXPECT_TRUE(info.isMeta_);
		EXPECT_EQ(3, info.metaVersion_);
		EXPECT_EQ(0, info.segNo_);
	}
	{
		NamespaceInfo info;
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/%FE%07", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/_meta/%FD%03", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/%FD%03", info));
		EXPECT_FALSE(NameComponents::extractInfo("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/audio/mic/hd/_meta/%FD%03", info));
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
