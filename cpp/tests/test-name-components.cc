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

TEST(TestNameComponents, TestNamespaceInfoExtraction)
{
	Name basePrefix("/my/base/prefix");
	string streamName = "mystream";
	NamespaceInfo ninfo;

	std::vector<Name> names;
	// stream prefix
	Name n1 = Name(basePrefix).append("ndnrtc").appendVersion(4).appendTimestamp(123).append(streamName);
	ninfo.reset();
	EXPECT_TRUE(NameComponents::extractInfo(n1, ninfo));
	EXPECT_EQ(ninfo.basePrefix_, basePrefix);
	EXPECT_EQ(ninfo.streamName_, streamName);
	EXPECT_EQ(ninfo.apiVersion_, 4);
	EXPECT_EQ(ninfo.streamTimestamp_, 123);

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
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
