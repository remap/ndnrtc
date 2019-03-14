//
// test-name-components.cc
//
//  Created by Peter Gusev on 22 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/regex.hpp>
#include <ndn-cpp/interest-filter.hpp>

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

TEST(TestNameComponents, TestRegex)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	std::vector<Name> names;
	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(0).append(streamName);

	// stream meta
	Name n2 = Name(n1).append(NameComponents::Meta);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_meta>$");
		EXPECT_TRUE(filter.doesMatch(n2));
	}
	// stream live meta
	Name n3 = Name(n1).append(NameComponents::Live).appendVersion(0);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_live>(<%FD[\\w%\+\-\.]+>)?$");
		EXPECT_TRUE(filter.doesMatch(n3));
	}
	// stream latest pointer
	Name n4 = Name(n1).append(NameComponents::Latest).appendVersion(0);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_latest>(<%FD[\\w%\+\-\.]+>)?$");
		EXPECT_TRUE(filter.doesMatch(n4));
	}

	// stream gop
	Name n5 = Name(n1).append(NameComponents::Gop).appendSequenceNumber(0).append(NameComponents::GopStart);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<start>$");
		EXPECT_TRUE(filter.doesMatch(n5));
	}

	Name n6 = Name(n1).append(NameComponents::Gop).appendSequenceNumber(0).append(NameComponents::GopEnd);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<end>$");
		EXPECT_TRUE(filter.doesMatch(n6));
	}

	// frame
	Name n7 = Name(n1).appendSequenceNumber(0);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<>{,2}");
		EXPECT_TRUE(filter.doesMatch(n7));
	}

	// frame meta
	Name n8 = Name(n7).append(NameComponents::Meta);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_meta>$");
		EXPECT_TRUE(filter.doesMatch(n8));
	}

	// frame manifest
	Name n9 = Name(n7).append(NameComponents::Manifest);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_manifest>$");
		EXPECT_TRUE(filter.doesMatch(n9));
	}

	// frame data
	Name n10 = Name(n7).appendSegment(0);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)(<%00[\\w%\+\-\.]+>)$");
		EXPECT_TRUE(filter.doesMatch(n10));
	}

	// frame parity
	Name n11 = Name(n7).append(NameComponents::Parity).appendSegment(0);
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_parity>(<%00[\\w%\+\-\.]+>)?$");
		EXPECT_TRUE(filter.doesMatch(n11));
	}

	names.push_back(n1);
	names.push_back(n2);
	names.push_back(n3);
	names.push_back(n4);
	names.push_back(n5);
	names.push_back(n6);
	names.push_back(n7);
	names.push_back(n8);
	names.push_back(n9);
	names.push_back(n10);
	names.push_back(n11);
	// check stream filter
	{
		InterestFilter filter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<>{,3}");

		for (auto &n : names)
			EXPECT_TRUE(filter.doesMatch(n));
	}
}

TEST(TestNameComponents, TestPrefixStream)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	EXPECT_TRUE(NameComponents::extractInfo(n1, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n1.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n1.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n1.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n1);
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n1);
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n1);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), Name());
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), Name());
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n1.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n1.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n1.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n1);
}

TEST(TestNameComponents, TestPrefixStreamMeta)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	// stream meta
	Name n2 = Name(n1).append(NameComponents::Meta);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n2, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Meta);
	EXPECT_EQ(ninfo.version_, 0);
	EXPECT_FALSE(ninfo.hasSeqNo_);
	EXPECT_FALSE(ninfo.hasSegNo_);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n2.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n2.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n2.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n2.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n2.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n2);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n2.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n2.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n2.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n2.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n2.getSubName(-5));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n2);

}

TEST(TestNameComponents, TestPrefixStreamMetaLive)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	// stream live meta
	Name n3 = Name(n1).append(NameComponents::Live).appendVersion(123);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n3, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Meta);
	EXPECT_EQ(ninfo.version_, 123);
	EXPECT_TRUE(ninfo.hasVersion_);
	EXPECT_FALSE(ninfo.hasSeqNo_);
	EXPECT_FALSE(ninfo.hasSegNo_);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n3.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n3.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n3.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n3.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n3.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n3);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n3.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n3.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n3.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n3.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n3.getSubName(-6));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n3);
}

TEST(TestNameComponents, TestPrefixStreamLatestPtr)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	// stream latest pointer
	Name n4 = Name(n1).append(NameComponents::Latest).appendVersion(123);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n4, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Pointer);
	EXPECT_EQ(ninfo.version_, 123);
	EXPECT_TRUE(ninfo.hasVersion_);
	EXPECT_FALSE(ninfo.hasSeqNo_);
	EXPECT_FALSE(ninfo.hasSegNo_);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n4.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n4.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n4.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n4.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n4.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n4);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n4.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n4.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n4.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n4.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n4.getSubName(-6));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n4);
}

TEST(TestNameComponents, TestPrefixStreamGop)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	// stream gop
	Name n5 = Name(n1).append(NameComponents::Gop).appendSequenceNumber(123).append(NameComponents::GopStart);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n5, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Pointer);
	EXPECT_FALSE(ninfo.hasVersion_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_FALSE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.isGopStart_);
	EXPECT_FALSE(ninfo.isGopEnd_);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n5.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n5.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n5.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n5.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n5.getPrefix(9));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n5);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n5.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n5.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n5.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n5.getSubName(-5));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n5.getSubName(-7));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n5);

	Name n6 = Name(n1).append(NameComponents::Gop).appendSequenceNumber(123).append(NameComponents::GopEnd);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n6, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Pointer);
	EXPECT_FALSE(ninfo.hasVersion_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_FALSE(ninfo.hasSegNo_);
	EXPECT_FALSE(ninfo.isGopStart_);
	EXPECT_TRUE(ninfo.isGopEnd_);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n6.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n6.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n6.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n6.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n6.getPrefix(9));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n6);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n6.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n6.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n6.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n6.getSubName(-5));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n6.getSubName(-7));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n6);
}
TEST(TestNameComponents, TestPrefixStreamFrame)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);

	// frame
	Name n7 = Name(n1).appendSequenceNumber(123);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n7, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_FALSE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Unknown);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n7.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n7.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n7.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n7.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n7.getPrefix(8));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n7);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), Name());
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n7.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n7.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n7.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n7.getSubName(-5));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n7);
}
TEST(TestNameComponents, TestPrefixStreamFrameMeta)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);
	Name n7 = Name(n1).appendSequenceNumber(123);

	// frame meta
	Name n8 = Name(n7).append(NameComponents::Meta);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n8, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_FALSE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Meta);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n8.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n8.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n8.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n8.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n8.getPrefix(8));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n8);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n8.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n8.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n8.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n8.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n8.getSubName(-6));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n8);
}
TEST(TestNameComponents, TestPrefixStreamFrameManifest)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);
	Name n7 = Name(n1).appendSequenceNumber(123);

	// frame manifest
	Name n9 = Name(n7).append(NameComponents::Manifest);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n9, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_FALSE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Manifest);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n9.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n9.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n9.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n9.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n9.getPrefix(8));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n9);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n9.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n9.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n9.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n9.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n9.getSubName(-6));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n9);
}
TEST(TestNameComponents, TestPrefixStreamFramePayload)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);
	Name n7 = Name(n1).appendSequenceNumber(123);

	// frame data
	Name n10 = Name(n7).appendSegment(123);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n10, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_TRUE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Data);
	EXPECT_EQ(ninfo.segNo_, 123);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n10.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n10.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n10.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n10.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n10.getPrefix(8));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n10);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n10.getSubName(-1));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n10.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n10.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n10.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n10.getSubName(-6));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n10);
}
TEST(TestNameComponents, TestPrefixStreamFrameParity)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);
	Name n7 = Name(n1).appendSequenceNumber(123);

	// frame parity
	Name n11 = Name(n7).append(NameComponents::Parity).appendSegment(123);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n11, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);
	EXPECT_EQ(ninfo.sampleNo_, 123);
	EXPECT_TRUE(ninfo.hasSegNo_);
	EXPECT_TRUE(ninfo.hasSeqNo_);
	EXPECT_EQ(ninfo.segmentClass_, SegmentClass::Parity);
	EXPECT_EQ(ninfo.segNo_, 123);

	EXPECT_EQ(ninfo.getPrefix(NameFilter::Base), n11.getPrefix(3));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Library), n11.getPrefix(5));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Timestamp), n11.getPrefix(6));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Stream), n11.getPrefix(7));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Sample), n11.getPrefix(8));
	EXPECT_EQ(ninfo.getPrefix(NameFilter::Segment), n11);

	EXPECT_EQ(ninfo.getSuffix(NameFilter::Segment), n11.getSubName(-2));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Sample), n11.getSubName(-3));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Stream), n11.getSubName(-4));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Timestamp), n11.getSubName(-5));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Library), n11.getSubName(-7));
	EXPECT_EQ(ninfo.getSuffix(NameFilter::Base), n11);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
