//
// test-packets.cc
//
//  Created by Peter Gusev on 14 March 2019.
//  Copyright 2019 Regents of the University of California
//

#include <stdlib.h>

#include <ndn-cpp/data.hpp>
#include <ndn-cpp/delegation-set.hpp>

#include "gtest/gtest.h"
#include "src/packets.hpp"
#include "src/proto/ndnrtc.pb.h"
#include "src/clock.hpp"
#include "src/network-data.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnrtc::packets;

TEST(TestPackets, TestStreamMeta)
{
    StreamMeta meta;
    meta.set_bitrate(2000);
    meta.set_description("stream description");
    meta.set_gop_size(30);
    meta.set_width(1920);
    meta.set_height(1080);

    Name prefix = NameComponents::streamPrefix("/my/base/prefix", "stream").append(NameComponents::Meta);
    boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
    d->setContent((uint8_t*)meta.SerializeAsString().data(), meta.SerializeAsString().size());

    NamespaceInfo ni;
    NameComponents::extractInfo(prefix, ni);

    Meta m(ni, d);
    EXPECT_EQ(m.getStreamMeta().bitrate(), meta.bitrate());
    EXPECT_EQ(m.getStreamMeta().description(), meta.description());
    EXPECT_EQ(m.getStreamMeta().gop_size(), meta.gop_size());
    EXPECT_EQ(m.getStreamMeta().width(), meta.width());
    EXPECT_EQ(m.getStreamMeta().height(), meta.height());
}

TEST(TestPackets, TestStreamLive)
{
    uint64_t nsts = clock::nanosecondTimestamp();
    int64_t secs = nsts/1E9;
    int32_t nanosec = nsts - secs*1E9;
    google::protobuf::Timestamp *timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(secs);
    timestamp->set_nanos(nanosec);

    LiveMeta meta;

    meta.set_allocated_timestamp(timestamp);
    meta.set_framerate(27);
    meta.set_segnum_delta(3);
    meta.set_segnum_key(5);
    meta.set_segnum_delta_parity(3);
    meta.set_segnum_key_parity(5);

    Name prefix = NameComponents::streamPrefix("/my/base/prefix", "stream").append(NameComponents::Live);
    boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
    d->setContent((uint8_t*)meta.SerializeAsString().data(), meta.SerializeAsString().size());

    NamespaceInfo ni;
    NameComponents::extractInfo(prefix, ni);

    Meta m(ni, d);
    EXPECT_EQ(m.getLiveMeta().timestamp().nanos(), meta.timestamp().nanos());
    EXPECT_EQ(m.getLiveMeta().timestamp().seconds(), meta.timestamp().seconds());
    EXPECT_EQ(m.getLiveMeta().framerate(), meta.framerate());
    EXPECT_EQ(m.getLiveMeta().segnum_delta(), meta.segnum_delta());
    EXPECT_EQ(m.getLiveMeta().segnum_key(), meta.segnum_key());
    EXPECT_EQ(m.getLiveMeta().segnum_delta_parity(), meta.segnum_delta_parity());
    EXPECT_EQ(m.getLiveMeta().segnum_key_parity(), meta.segnum_key_parity());
}

TEST(TestPackets, TestFrameMeta)
{
    uint64_t nsts = clock::nanosecondTimestamp();
    int64_t secs = nsts/1E9;
    int32_t nanosec = nsts - secs*1E9;
    google::protobuf::Timestamp *timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(secs);
    timestamp->set_nanos(nanosec);

    FrameMeta meta;

    meta.set_allocated_capture_timestamp(timestamp);
    meta.set_parity_size(3);
    meta.set_gop_number(31);
    meta.set_gop_position(5);
    meta.set_type(FrameMeta_FrameType_Delta);
    meta.set_generation_delay(0);

    ndntools::ContentMetaInfo metaInfo;
    metaInfo.setContentType("ndnrtc/4")
            .setTimestamp(secs)
            .setHasSegments(true)
            .setOther(Blob::fromRawStr(meta.SerializeAsString()));

    Name prefix = NameComponents::streamPrefix("/my/base/prefix", "stream").appendSequenceNumber(936).append(NameComponents::Meta);
    boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
    d->setContent(metaInfo.wireEncode());

    NamespaceInfo ni;
    NameComponents::extractInfo(prefix, ni);

    Meta m(ni, d);
    EXPECT_EQ(m.getContentMetaInfo().getTimestamp(), metaInfo.getTimestamp());
    EXPECT_EQ(m.getContentMetaInfo().getContentType(), metaInfo.getContentType());
    EXPECT_EQ(m.getContentMetaInfo().getOther().toRawStr(), metaInfo.getOther().toRawStr());
    EXPECT_EQ(m.getFrameMeta().capture_timestamp().nanos(), meta.capture_timestamp().nanos());
    EXPECT_EQ(m.getFrameMeta().capture_timestamp().seconds(), meta.capture_timestamp().seconds());
    EXPECT_EQ(m.getFrameMeta().parity_size(), meta.parity_size());
    EXPECT_EQ(m.getFrameMeta().gop_number(), meta.gop_number());
    EXPECT_EQ(m.getFrameMeta().gop_position(), meta.gop_position());
    EXPECT_EQ(m.getFrameMeta().type(), meta.type());
    EXPECT_EQ(m.getFrameMeta().generation_delay(), meta.generation_delay());
}

TEST(TestPackets, TestLatest)
{
    Name stream = NameComponents::streamPrefix("/my/base/prefix", "stream");
    Name prefix = Name(stream).append(NameComponents::Latest)
                              .appendVersion(123);
    Name p1 = Name(stream).appendSequenceNumber(123);
    Name p2 = Name(stream).append(NameComponents::Gop).appendSequenceNumber(123);

    DelegationSet set;
    set.add(0,p1);
    set.add(1,p2);

    boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
    d->setContent(set.wireEncode());

    NamespaceInfo ni;
    NameComponents::extractInfo(prefix, ni);
    Pointer p(ni, d);

    EXPECT_EQ(p.getDelegationSet().get(0).getName(), p1);
    EXPECT_EQ(p.getDelegationSet().get(1).getName(), p2);
}

TEST(TestPackets, TestGopPointers)
{
    Name stream = NameComponents::streamPrefix("/my/base/prefix", "stream");
    Name p1 = Name(stream).appendSequenceNumber(123);
    Name p2 = Name(stream).appendSequenceNumber(153);

    {
        Name prefix = Name(stream).append(NameComponents::Gop)
                                  .append(NameComponents::GopStart);
        DelegationSet set;
        set.add(0,p1);

        boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
        d->setContent(set.wireEncode());

        NamespaceInfo ni;
        NameComponents::extractInfo(prefix, ni);
        Pointer p(ni, d);

        EXPECT_EQ(p.getDelegationSet().get(0).getName(), p1);
    }
    {
        Name prefix = Name(stream).append(NameComponents::Gop)
                                  .append(NameComponents::GopEnd);
        DelegationSet set;
        set.add(0,p2);

        boost::shared_ptr<Data> d = boost::make_shared<Data>(prefix);
        d->setContent(set.wireEncode());

        NamespaceInfo ni;
        NameComponents::extractInfo(prefix, ni);
        Pointer p(ni, d);

        EXPECT_EQ(p.getDelegationSet().get(0).getName(), p2);
    }
}

TEST(TestPackets, TestManifest)
{
    Name stream = NameComponents::streamPrefix("/my/base/prefix", "stream");
    Name frame = Name(stream).appendSequenceNumber(123);
    vector<boost::shared_ptr<Data>> packets;
    for (int i = 0 ; i < 100; ++ i)
    {
        boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(frame).appendSegment(i));
        string content = "random string"+to_string(i);
        d->setContent(Blob::fromRawStr(content));
        packets.push_back(d);
    }

    boost::shared_ptr<Data> manifest = SegmentsManifest::packManifest(Name(frame).append(NameComponents::Manifest), packets);
    NamespaceInfo ni;
    NameComponents::extractInfo(manifest->getName(), ni);

    ndnrtc::packets::Manifest m(ni, manifest);
    EXPECT_EQ(m.getSize(), 100);

    for (auto d:packets)
        EXPECT_TRUE(m.hasData(*d));

}

TEST(TestPackets, TestData)
{
    Name stream = NameComponents::streamPrefix("/my/base/prefix", "stream");
    Name frame = Name(stream).appendSequenceNumber(123);

    boost::shared_ptr<Data> seg = boost::make_shared<Data>(Name(frame).appendSegment(0));
    seg->setContent(Blob::fromRawStr("test content"));
    seg->getMetaInfo().setFinalBlockId(Name::Component::fromSegment(5));

    NamespaceInfo ni;
    NameComponents::extractInfo(seg->getName(), ni);

    Segment s(ni, seg);
    EXPECT_EQ(s.getTotalSegmentsNum(), 6);
}

TEST(TestPackets, TestParity)
{
    Name stream = NameComponents::streamPrefix("/my/base/prefix", "stream");
    Name frame = Name(stream).appendSequenceNumber(123);

    boost::shared_ptr<Data> seg = boost::make_shared<Data>(Name(frame).append(NameComponents::Parity)
                                                                      .appendSegment(0));
    seg->setContent(Blob::fromRawStr("test content"));
    seg->getMetaInfo().setFinalBlockId(Name::Component::fromSegment(5));

    NamespaceInfo ni;
    NameComponents::extractInfo(seg->getName(), ni);

    Segment s(ni, seg);
    EXPECT_EQ(s.getTotalSegmentsNum(), 6);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
