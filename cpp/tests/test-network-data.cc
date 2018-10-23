//
// test-network-data.cc
//
//  Created by Peter Gusev on 09 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <ctime>
#include <boost/move/move.hpp>
#include <boost/assign.hpp>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <ndn-cpp/digest-sha256-signature.hpp>
#include <ndn-cpp/name.hpp>

#include "tests-helpers.hpp"
#include "gtest/gtest.h"
#include "src/frame-data.hpp"

using namespace ndnrtc;

class DataPacketTest : public DataPacket
{
  public:
    DataPacketTest(unsigned int dataLength, const uint8_t *payload) : DataPacket(dataLength, payload) {}
    DataPacketTest(const std::vector<uint8_t> &payload) : DataPacket(payload) {}
    DataPacketTest(const DataPacketTest &dataPacket) : DataPacket(dataPacket) {}
    DataPacketTest(NetworkData &&networkData) : DataPacket(boost::move(networkData)) {}

    unsigned int getBlobsNum() const { return DataPacket::getBlobsNum(); }
    const Blob getBlob(size_t pos) const { return DataPacket::getBlob(pos); }
    void addBlob(uint16_t dataLength, const uint8_t *data) { DataPacket::addBlob(dataLength, data); }
};

TEST(TestNetworkData, TestCreate1)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;

    NetworkData nd(data_len, data);

    EXPECT_EQ(data_len, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());
}

TEST(TestNetworkData, TestCreate2)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    std::size_t const sensible_data_len = data_len - 1;
    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;
    std::vector<uint8_t> v(&data[0], &data[data_len - 1]);

    NetworkData nd(v);

    EXPECT_EQ(sensible_data_len, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());
}

TEST(TestNetworkData, TestCreateWithEmptyVector)
{
    std::vector<uint8_t> v;
    NetworkData nd(v);
    EXPECT_TRUE(nd.isValid());
}

TEST(TestNetworkData, TestCopy)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;

    NetworkData nd(data_len, data);
    NetworkData nd2(nd);

    EXPECT_EQ(nd.getLength(), nd2.getLength());
    for (int i = 0; i < nd2.getLength(); ++i)
        EXPECT_EQ(nd.getData()[i], nd2.getData()[i]);
    EXPECT_EQ(crcExpected, nd2.getCrcValue());
}

TEST(TestNetworkData, TestMove)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;

    NetworkData nd(data_len, data);
    NetworkData nd2(boost::move(nd));

    EXPECT_EQ(0, nd.getLength());
    EXPECT_EQ(nullptr, nd.getData());
    EXPECT_EQ(data_len, nd2.getLength());
    for (int i = 0; i < nd2.getLength(); ++i)
        EXPECT_EQ(data[i], nd2.getData()[i]);
    EXPECT_EQ(crcExpected, nd2.getCrcValue());
}

TEST(TestNetworkData, TestSwap)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    uint8_t const data2[] = {0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len2 = sizeof(data2) / sizeof(data2[0]);

    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;
    boost::uint16_t const crcExpected2 = 0x2A64;

    NetworkData nd(data_len, data);
    NetworkData nd2(data_len2, data2);

    std::swap(nd, nd2);

    EXPECT_EQ(data_len2, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(data2[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected2, nd.getCrcValue());

    EXPECT_EQ(data_len, nd2.getLength());
    for (int i = 0; i < nd2.getLength(); ++i)
        EXPECT_EQ(data[i], nd2.getData()[i]);
    EXPECT_EQ(crcExpected, nd2.getCrcValue());

    nd.swap(nd2);

    EXPECT_EQ(data_len2, nd2.getLength());
    for (int i = 0; i < nd2.getLength(); ++i)
        EXPECT_EQ(data2[i], nd2.getData()[i]);
    EXPECT_EQ(crcExpected2, nd2.getCrcValue());

    EXPECT_EQ(data_len, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());
}

TEST(TestNetworkData, TestAssign)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    uint8_t const data2[] = {0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len2 = sizeof(data2) / sizeof(data2[0]);

    // The expected CRC for the given data
    boost::uint16_t const crcExpected = 0xBB3D;
    boost::uint16_t const crcExpected2 = 0x2A64;

    NetworkData nd(data_len, data);
    NetworkData nd2(data_len2, data2);

    nd2 = nd;

    EXPECT_EQ(nd2.getLength(), nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(nd2.getData()[i], nd.getData()[i]);
    EXPECT_EQ(nd2.getCrcValue(), nd.getCrcValue());
}

TEST(TestNetworkData, TestLowerBoundary)
{
    uint8_t const data[] = {0x31};
    std::size_t const data_len = 1;
    boost::uint16_t const crcExpected = 0xD4C1;

    NetworkData nd(NetworkData(data_len, data));
    EXPECT_EQ(data_len, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
        EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());
}

TEST(TestDataPacket, TestCreate)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);

    DataPacketTest dp(data_len, data);

    EXPECT_EQ(data_len + 1, dp.getLength());
    EXPECT_EQ(data_len, dp.getPayload().size());
    for (int i = 0; i < dp.getPayload().size(); ++i)
        EXPECT_EQ(data[i], dp.getPayload()[i]);
    EXPECT_EQ(0, dp.getBlobsNum());
    EXPECT_EQ(true, dp.isValid());
    EXPECT_EQ(DataPacket::wireLength(data_len, 0), dp.getLength());
}

TEST(TestDataPacket, TestCreateEmpty)
{
    {
        std::vector<uint8_t> v;
        DataPacketTest dp(v);
        EXPECT_EQ(1, dp.getLength());
        EXPECT_TRUE(dp.isValid());
        EXPECT_EQ(0, dp.getBlobsNum());
        EXPECT_EQ(0, dp.getPayload().size());
    }
    {
        std::vector<uint8_t> v;
        DataPacketTest dp(v);
        int number = 5;
        dp.addBlob(sizeof(number), (uint8_t *)&number);
        EXPECT_EQ(1, dp.getBlobsNum());
        EXPECT_EQ(number, *(int *)dp.getBlob(0).data());
        EXPECT_TRUE(dp.isValid());
    }
}

TEST(TestDataPacket, TestBlobs)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    uint8_t const header1[] = {0x31};
    std::size_t const header1_data_len = sizeof(header1) / sizeof(header1[0]);
    uint8_t const header2[] = {0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const header2_data_len = sizeof(header2) / sizeof(header2[0]);
    uint8_t const header3[] = {0x31, 0x32, 0x33, 0x34};
    std::size_t const header3_data_len = sizeof(header3) / sizeof(header3[0]);

    DataPacketTest dp(data_len, data);

    dp.addBlob(header1_data_len, header1);
    dp.addBlob(header2_data_len, header2);
    dp.addBlob(header3_data_len, header3);

    int x = 5;
    dp.addBlob(sizeof(int), (const uint8_t *)&x);

    ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(data_len + header1_data_len + header2_data_len + header3_data_len + sizeof(int) + 4 * sizeof(uint16_t) + 1, dp.getLength());
    EXPECT_EQ(data_len, dp.getPayload().size());
    for (int i = 0; i < dp.getPayload().size(); ++i)
        EXPECT_EQ(data[i], dp.getPayload()[i]);
    EXPECT_EQ(header1_data_len, dp.getBlob(0).size());
    for (int i = 0; i < dp.getBlob(0).size(); ++i)
        EXPECT_EQ(header1[i], dp.getBlob(0)[i]);
    EXPECT_EQ(header2_data_len, dp.getBlob(1).size());
    for (int i = 0; i < dp.getBlob(1).size(); ++i)
        EXPECT_EQ(header2[i], dp.getBlob(1)[i]);
    EXPECT_EQ(header3_data_len, dp.getBlob(2).size());
    for (int i = 0; i < dp.getBlob(2).size(); ++i)
        EXPECT_EQ(header3[i], dp.getBlob(2)[i]);
    EXPECT_EQ(sizeof(int), dp.getBlob(3).size());
    int y = *((int *)dp.getBlob(3).data());
    EXPECT_EQ(x, y);
    std::vector<size_t> blobLengths = boost::assign::list_of(header1_data_len)(header2_data_len)(header3_data_len)(sizeof(int));
    EXPECT_EQ(DataPacket::wireLength(data_len, blobLengths), dp.getLength());
}

TEST(TestDataPacket, TestDataPacketCopy)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    uint8_t const header1[] = {0x31};
    std::size_t const header1_data_len = sizeof(header1) / sizeof(header1[0]);
    uint8_t const header2[] = {0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const header2_data_len = sizeof(header2) / sizeof(header2[0]);
    uint8_t const header3[] = {0x31, 0x32, 0x33, 0x34};
    std::size_t const header3_data_len = sizeof(header3) / sizeof(header3[0]);

    DataPacketTest dp0(data_len, data);

    dp0.addBlob(header1_data_len, header1);
    EXPECT_TRUE(dp0.isValid());
    dp0.addBlob(header2_data_len, header2);
    EXPECT_TRUE(dp0.isValid());
    dp0.addBlob(header3_data_len, header3);
    EXPECT_TRUE(dp0.isValid());

    int x = 5;
    dp0.addBlob(sizeof(int), (const uint8_t *)&x);
    EXPECT_TRUE(dp0.isValid());

    DataPacketTest dp(dp0);

    ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(data_len + header1_data_len + header2_data_len + header3_data_len + sizeof(int) + 4 * sizeof(uint16_t) + 1, dp.getLength());
    EXPECT_EQ(data_len, dp.getPayload().size());
    bool identical = true;
    for (int i = 0; i < dp.getPayload().size() && identical; ++i)
        identical &= (data[i] == dp.getPayload()[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header1_data_len, dp.getBlob(0).size());
    for (int i = 0; i < dp.getBlob(0).size() && identical; ++i)
        identical &= (header1[i] == dp.getBlob(0)[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header2_data_len, dp.getBlob(1).size());
    for (int i = 0; i < dp.getBlob(1).size() && identical; ++i)
        identical = (header2[i] == dp.getBlob(1)[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header3_data_len, dp.getBlob(2).size());
    for (int i = 0; i < dp.getBlob(2).size() && identical; ++i)
        identical = (header3[i] == dp.getBlob(2)[i]);
    EXPECT_TRUE(identical);

    EXPECT_EQ(sizeof(int), dp.getBlob(3).size());
    int y = *((int *)dp.getBlob(3).data());
    EXPECT_EQ(x, y);
}

TEST(TestDataPacket, TestDataPacketAssign)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    uint8_t const header1[] = {0x31};
    std::size_t const header1_data_len = sizeof(header1) / sizeof(header1[0]);
    uint8_t const header2[] = {0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const header2_data_len = sizeof(header2) / sizeof(header2[0]);
    uint8_t const header3[] = {0x31, 0x32, 0x33, 0x34};
    std::size_t const header3_data_len = sizeof(header3) / sizeof(header3[0]);

    DataPacketTest dp0(data_len, data);

    dp0.addBlob(header1_data_len, header1);
    dp0.addBlob(header2_data_len, header2);
    dp0.addBlob(header3_data_len, header3);

    int x = 5;
    dp0.addBlob(sizeof(int), (const uint8_t *)&x);

    DataPacketTest dp = dp0;

    ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(data_len + header1_data_len + header2_data_len + header3_data_len + sizeof(int) + 4 * sizeof(uint16_t) + 1, dp.getLength());
    EXPECT_EQ(data_len, dp.getPayload().size());
    bool identical = true;
    for (int i = 0; i < dp.getPayload().size() && identical; ++i)
        identical &= (data[i] == dp.getPayload()[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header1_data_len, dp.getBlob(0).size());
    for (int i = 0; i < dp.getBlob(0).size() && identical; ++i)
        identical &= (header1[i] == dp.getBlob(0)[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header2_data_len, dp.getBlob(1).size());
    for (int i = 0; i < dp.getBlob(1).size() && identical; ++i)
        identical = (header2[i] == dp.getBlob(1)[i]);
    EXPECT_TRUE(identical);

    identical = true;
    EXPECT_EQ(header3_data_len, dp.getBlob(2).size());
    for (int i = 0; i < dp.getBlob(2).size() && identical; ++i)
        identical = (header3[i] == dp.getBlob(2)[i]);
    EXPECT_TRUE(identical);

    EXPECT_EQ(sizeof(int), dp.getBlob(3).size());
    int y = *((int *)dp.getBlob(3).data());
    EXPECT_EQ(x, y);
}

TEST(TestDataPacket, TestMoveNetworkData)
{
    uint8_t const data[] = {0x01, 0x04, 0x00, 't', 'e', 's', 't',
                            0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);

    NetworkData nd(data_len, data);
    DataPacketTest dp(boost::move(nd));

    EXPECT_EQ(0, nd.getLength());
    EXPECT_EQ(nullptr, nd.getData());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(1, dp.getBlobsNum());
    EXPECT_EQ("test", std::string((char *)dp.getBlob(0).data(), dp.getBlob(0).size()));
    EXPECT_EQ("123456789", std::string((char *)dp.getPayload().data(), dp.getPayload().size()));
}

TEST(TestDataPacket, TestCreateEmptyPacket)
{
    EXPECT_EQ(1, DataPacket::wireLength(0, 0));
    std::vector<size_t> v = boost::assign::list_of(0);
    EXPECT_EQ(1, DataPacket::wireLength(0, v));

    DataPacket dp(boost::move(NetworkData(std::vector<uint8_t>())));
    EXPECT_FALSE(dp.isValid());
}

TEST(TestDataPacket, TestInvalidRawData)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    NetworkData nd(data_len, data);
    DataPacketTest dp(boost::move(nd));

    EXPECT_FALSE(dp.isValid());
}

TEST(TestSamplePacket, TestCreate)
{
    {
        uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
        std::size_t const data_len = sizeof(data) / sizeof(data[0]);
        CommonSamplePacket sp(data_len, data);
        CommonHeader hdr = {25, 39936287, 1460399362};

        EXPECT_FALSE(sp.isValid());

        sp.setHeader(hdr);

        EXPECT_TRUE(sp.isValid());
        EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
        EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
        EXPECT_EQ(hdr.publishUnixTimestamp_, sp.getHeader().publishUnixTimestamp_);

        EXPECT_ANY_THROW(sp.setHeader(hdr));
    }
    {
        uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
        std::size_t const data_len = sizeof(data) / sizeof(data[0]);
        CommonHeader hdr = {25, 39936287, 1460399362};
        CommonSamplePacket sp(hdr, data_len, data);

        EXPECT_TRUE(sp.isValid());
        EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
        EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
        EXPECT_EQ(hdr.publishUnixTimestamp_, sp.getHeader().publishUnixTimestamp_);

        EXPECT_ANY_THROW(sp.setHeader(hdr));
    }
}

TEST(TestSamplePacket, TestCreateFromRaw)
{
    CommonHeader hdr = {25, 39936287, 1460399362};
    uint8_t const payload[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const payload_len = sizeof(payload) / sizeof(payload[0]);
    std::vector<uint8_t> data(&payload[0], &payload[0] + payload_len);
    data.insert(data.begin(), (uint8_t *)&hdr, (uint8_t *)&hdr + sizeof(CommonHeader));
    uint16_t sz = sizeof(CommonHeader);
    data.insert(data.begin(), (sz & 0xff00) >> 8);
    data.insert(data.begin(), (sz & 0x00ff));
    data.insert(data.begin(), 1);
    NetworkData nd(data);
    CommonSamplePacket sp(boost::move(nd));

    ASSERT_TRUE(sp.isValid());
    EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, sp.getHeader().publishUnixTimestamp_);
    EXPECT_EQ(payload_len, sp.getPayload().size());
    for (int i = 0; i < payload_len; i++)
        EXPECT_EQ(payload[i], sp.getPayload()[i]);
}

TEST(TestSamplePacket, TestCreateFromRawInvalid)
{
    uint8_t const payload[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    std::size_t const payload_len = sizeof(payload) / sizeof(payload[0]);
    std::vector<uint8_t> data(&payload[0], &payload[0] + payload_len);
    NetworkData nd(data);
    CommonSamplePacket sp(boost::move(nd));

    ASSERT_FALSE(sp.isValid());
}

TEST(TestAudioBundle, TestBundling)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    EXPECT_EQ(0, bundlePacket.getSamplesNum());
    EXPECT_GE(wire_len, bundlePacket.getRemainingSpace());

    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

    EXPECT_FALSE(sample.getHeader().isRtcp_);
    // EXPECT_EQ(1234, sample.getHeader().dummy_);
    ASSERT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), sample.size());
    ASSERT_EQ(data_len + sizeof(AudioSampleHeader), sample.size());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    EXPECT_ANY_THROW(bundlePacket << sample);
    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());
    for (int i = 0; i < bundlePacket.getSamplesNum(); ++i)
    {
        EXPECT_FALSE(bundlePacket[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, bundlePacket[i].getHeader().dummy_);
        for (int k = 0; k < bundlePacket[i].size() - sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], bundlePacket[i].data()[k]);
    }

    bundlePacket.clear();
    EXPECT_EQ(0, bundlePacket.getSamplesNum());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());
    ASSERT_FALSE(bundlePacket.isValid());
    bundlePacket.setHeader({25, 1, 2});
    ASSERT_TRUE(bundlePacket.isValid());
    EXPECT_GE(wire_len, bundlePacket.getLength());
    EXPECT_EQ(AudioBundlePacket::wireLength(wire_len, data_len), bundlePacket.getLength());

    NetworkData nd(boost::move(bundlePacket));
    EXPECT_FALSE(bundlePacket.isValid());

    AudioBundlePacket newBundle(boost::move(nd));
    EXPECT_TRUE(newBundle.isValid());
    EXPECT_FALSE(nd.isValid());

    AudioBundlePacket thirdBundle(wire_len);

    ASSERT_FALSE(bundlePacket.isValid());
    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              newBundle.getSamplesNum());
    EXPECT_EQ(25, newBundle.getHeader().sampleRate_);
    EXPECT_EQ(1, newBundle.getHeader().publishTimestampMs_);
    EXPECT_EQ(2, newBundle.getHeader().publishUnixTimestamp_);

    for (int i = 0; i < newBundle.getSamplesNum(); ++i)
    {
        EXPECT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), newBundle[i].size());
        EXPECT_FALSE(newBundle[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, newBundle[i].getHeader().dummy_);
        for (int k = 0; k < newBundle[i].size() - sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], newBundle[i].data()[k]);

        thirdBundle << newBundle[i];
    }

    thirdBundle.setHeader(newBundle.getHeader());
    for (int i = 0; i < thirdBundle.getLength(); ++i)
        EXPECT_EQ(newBundle.getData()[i], thirdBundle.getData()[i]);
}

TEST(TestAudioBundle, TestSwap)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    EXPECT_EQ(0, bundlePacket.getSamplesNum());
    EXPECT_GE(wire_len, bundlePacket.getRemainingSpace());

    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

    EXPECT_FALSE(sample.getHeader().isRtcp_);
    // EXPECT_EQ(1234, sample.getHeader().dummy_);
    ASSERT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), sample.size());
    ASSERT_EQ(data_len + sizeof(AudioSampleHeader), sample.size());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());
    for (int i = 0; i < bundlePacket.getSamplesNum(); ++i)
    {
        EXPECT_FALSE(bundlePacket[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, bundlePacket[i].getHeader().dummy_);
        for (int k = 0; k < bundlePacket[i].size() - sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], bundlePacket[i].data()[k]);
    }

    AudioBundlePacket aBundle(wire_len);
    aBundle.swap(bundlePacket);

    ASSERT_FALSE(bundlePacket.isValid());
    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              aBundle.getSamplesNum());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());
}

TEST(TestAudioBundle, TestBundlingUsingOperator)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    EXPECT_EQ(0, bundlePacket.getSamplesNum());
    EXPECT_GE(wire_len, bundlePacket.getRemainingSpace());

    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

    EXPECT_FALSE(sample.getHeader().isRtcp_);
    // EXPECT_EQ(1234, sample.getHeader().dummy_);
    ASSERT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), sample.size());
    ASSERT_EQ(data_len + sizeof(AudioSampleHeader), sample.size());

    bundlePacket << sample << sample << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());
    for (int i = 0; i < bundlePacket.getSamplesNum(); ++i)
    {
        EXPECT_FALSE(bundlePacket[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, bundlePacket[i].getHeader().dummy_);
        for (int k = 0; k < bundlePacket[i].size() - sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], bundlePacket[i].data()[k]);
    }
}

TEST(TestAudioBundle, TestMoveAndReuse)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());
    AudioBundlePacket tempBundle(wire_len);

    for (int j = 0; j < 3; ++j)
    {
        bundlePacket << sample << sample << sample;

        ASSERT_EQ(3, bundlePacket.getSamplesNum());
        for (int i = 0; i < bundlePacket.getSamplesNum(); ++i)
        {
            EXPECT_FALSE(bundlePacket[i].getHeader().isRtcp_);
            // EXPECT_EQ(1234, bundlePacket[i].getHeader().dummy_);
            bool identical = true;
            for (int k = 0; k < bundlePacket[i].size() - sizeof(AudioSampleHeader) && identical; ++k)
                identical = (rtpData[k] == bundlePacket[i].data()[k]);
            EXPECT_TRUE(identical);
        }

        // now move bundle elsewhere
        tempBundle.swap(bundlePacket);
        EXPECT_FALSE(bundlePacket.isValid());
        bundlePacket.clear();
        EXPECT_EQ(1, bundlePacket.getLength());
        ASSERT_EQ(3, tempBundle.getSamplesNum());
        for (int i = 0; i < tempBundle.getSamplesNum(); ++i)
        {
            EXPECT_FALSE(tempBundle[i].getHeader().isRtcp_);
            // EXPECT_EQ(1234, tempBundle[i].getHeader().dummy_);
            bool identical = true;
            for (int k = 0; k < tempBundle[i].size() - sizeof(AudioSampleHeader) && identical; ++k)
                identical = (rtpData[k] == tempBundle[i].data()[k]);
            EXPECT_TRUE(identical);
        }
    }
}

TEST(TestDataSegment, TestSlice)
{
    int data_len = 6472;
    std::vector<uint8_t> data;

    for (int i = 0; i < data_len; ++i)
        data.push_back((uint8_t)i);

    NetworkData nd((const std::vector<uint8_t>)data);
    int wireLength = 1000;
    std::vector<VideoFrameSegment> segments = VideoFrameSegment::slice(nd, wireLength);
    int payloadLength = VideoFrameSegment::payloadLength(wireLength);

    EXPECT_EQ(VideoFrameSegment::numSlices(nd, wireLength), segments.size());

    int idx = 0;
    VideoFrameSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    header.totalSegmentsNum_ = segments.size();
    header.playbackNo_ = 0;
    header.pairedSequenceNo_ = 1;
    header.paritySegmentsNum_ = 2;

    for (auto &s : segments)
    {
        VideoFrameSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        hdr.interestArrivalMs_ += idx;
        hdr.playbackNo_ += idx;
        idx++;
        s.setHeader(hdr);
    }

    for (int i = 0; i < data_len; ++i)
    {
        size_t segIdx = i / payloadLength;
        VideoFrameSegment s = segments[segIdx];
        size_t pos = i % payloadLength;
        EXPECT_EQ(data[i], s.getPayload()[pos]);
    }

    idx = 0;
    size_t lastSegmentPayloadSize = data_len - (data_len / payloadLength) * payloadLength;
    for (std::vector<VideoFrameSegment>::iterator it = segments.begin(); it < segments.end(); ++it)
    {
        if (it + 1 == segments.end())
        {
            EXPECT_EQ(VideoFrameSegment::wireLength(lastSegmentPayloadSize), it->getNetworkData()->getLength());
            EXPECT_EQ(VideoFrameSegment::wireLength(lastSegmentPayloadSize), it->size());
        }
        else
        {
            EXPECT_EQ(wireLength, it->getNetworkData()->getLength());
            EXPECT_EQ(wireLength, it->size());
        }

        EXPECT_EQ(header.interestNonce_ + idx, it->getHeader().interestNonce_);
        EXPECT_EQ(header.interestArrivalMs_ + idx, it->getHeader().interestArrivalMs_);
        EXPECT_EQ(header.generationDelayMs_, it->getHeader().generationDelayMs_);
        EXPECT_EQ(header.totalSegmentsNum_, it->getHeader().totalSegmentsNum_);
        EXPECT_EQ(header.playbackNo_ + idx, it->getHeader().playbackNo_);
        EXPECT_EQ(header.pairedSequenceNo_, it->getHeader().pairedSequenceNo_);
        EXPECT_EQ(header.paritySegmentsNum_, it->getHeader().paritySegmentsNum_);
        idx++;
    }
}

TEST(TestVideoFramePacket, TestCreate)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);
    EXPECT_EQ(frame._encodedWidth, fp.getFrame()._encodedWidth);
    EXPECT_EQ(frame._encodedHeight, fp.getFrame()._encodedHeight);
    EXPECT_EQ(frame._timeStamp, fp.getFrame()._timeStamp);
    EXPECT_EQ(frame.capture_time_ms_, fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType, fp.getFrame()._frameType);
    EXPECT_EQ(frame._completeFrame, fp.getFrame()._completeFrame);

    GT_PRINTF("frame packet length is %d\n", fp.getLength());

    EXPECT_ANY_THROW(fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2));

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    fp.setHeader(hdr);
    ASSERT_TRUE(fp.isValid());

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, fp.getHeader().publishUnixTimestamp_);

    int length = fp.getLength();
    boost::shared_ptr<NetworkData> parityData = fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    EXPECT_TRUE(parityData.get());
    EXPECT_EQ(length, fp.getLength());
    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, fp.getHeader().publishUnixTimestamp_);
    EXPECT_EQ(frame._encodedWidth, fp.getFrame()._encodedWidth);
    EXPECT_EQ(frame._encodedHeight, fp.getFrame()._encodedHeight);
    EXPECT_EQ(frame._timeStamp, fp.getFrame()._timeStamp);
    EXPECT_EQ(frame.capture_time_ms_, fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType, fp.getFrame()._frameType);
    EXPECT_EQ(frame._completeFrame, fp.getFrame()._completeFrame);
    for (int i = 0; i < frameLen; ++i)
        EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
}

TEST(TestVideoFramePacket, TestCreateEmpty)
{
    VideoFramePacket vp(boost::move(NetworkData(std::vector<uint8_t>())));
    EXPECT_FALSE(vp.isValid());
}

TEST(TestVideoFramePacket, TestAddSyncList)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    fp.setSyncList(syncList);
    EXPECT_EQ(syncList, fp.getSyncList());

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    fp.setHeader(hdr);
    EXPECT_EQ(syncList, fp.getSyncList());

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, fp.getHeader().publishUnixTimestamp_);

    int length = fp.getLength();
    boost::shared_ptr<NetworkData> parityData = fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    EXPECT_TRUE(parityData.get());
    EXPECT_EQ(length, fp.getLength());
    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, fp.getHeader().publishUnixTimestamp_);
    EXPECT_EQ(frame._encodedWidth, fp.getFrame()._encodedWidth);
    EXPECT_EQ(frame._encodedHeight, fp.getFrame()._encodedHeight);
    EXPECT_EQ(frame._timeStamp, fp.getFrame()._timeStamp);
    EXPECT_EQ(frame.capture_time_ms_, fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType, fp.getFrame()._frameType);
    EXPECT_EQ(frame._completeFrame, fp.getFrame()._completeFrame);
    for (int i = 0; i < frameLen; ++i)
        EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
}

TEST(TestVideoFramePacket, TestFromNetworkData)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    VideoFramePacket first(frame);
    first.setSyncList(syncList);
    first.setHeader(hdr);

    VideoFramePacket fp(boost::move((NetworkData &)first));

    EXPECT_EQ(0, first.getLength());
    EXPECT_FALSE(first.isValid());
    EXPECT_EQ(syncList, fp.getSyncList());

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, fp.getHeader().publishUnixTimestamp_);
    EXPECT_EQ(frame._encodedWidth, fp.getFrame()._encodedWidth);
    EXPECT_EQ(frame._encodedHeight, fp.getFrame()._encodedHeight);
    EXPECT_EQ(frame._timeStamp, fp.getFrame()._timeStamp);
    EXPECT_EQ(frame.capture_time_ms_, fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType, fp.getFrame()._frameType);
    EXPECT_EQ(frame._completeFrame, fp.getFrame()._completeFrame);
    for (int i = 0; i < frameLen; ++i)
        EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
}

TEST(TestVideoFramePacket, TestAddSyncListThrow)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    fp.setHeader(hdr);

    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    EXPECT_ANY_THROW(fp.setSyncList(syncList));
}

TEST(TestVideoFramePacket, TestSliceFrame)
{
    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    VideoFramePacket vp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    vp.setSyncList(syncList);
    vp.setHeader(hdr);
    boost::shared_ptr<NetworkData> parity = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

    std::vector<VideoFrameSegment> frameSegments = VideoFrameSegment::slice(vp, 1000);
    std::vector<CommonSegment> paritySegments = CommonSegment::slice(*parity, 1000);

    EXPECT_EQ(VideoFrameSegment::numSlices(vp, 1000), frameSegments.size());
    EXPECT_EQ(CommonSegment::numSlices(*parity, 1000), paritySegments.size());
    EXPECT_EQ(5, frameSegments.size());
    EXPECT_EQ(1, paritySegments.size());
}

// test for data relocation after retrieving parity data which leads to BAD_ACCESS
// when accessing packet blobs
TEST(TestVideoFramePacket, TestGetParity)
{
    std::srand(std::time(0));
    for (int i = 0; i < 100; ++i)
    {
        CommonHeader hdr;
        hdr.sampleRate_ = 24.7;
        hdr.publishTimestampMs_ = 488589553;
        hdr.publishUnixTimestamp_ = 1460488589;

        size_t frameLen = std::rand() % 30000 + 100;
        int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
        uint8_t *buffer = (uint8_t *)malloc(frameLen);
        for (int i = 0; i < frameLen; ++i)
            buffer[i] = i % 255;

        webrtc::EncodedImage frame(buffer, frameLen, size);
        frame._encodedWidth = 640;
        frame._encodedHeight = 480;
        frame._timeStamp = 1460488589;
        frame.capture_time_ms_ = 1460488569;
        frame._frameType = webrtc::kVideoFrameKey;
        frame._completeFrame = true;

        VideoFramePacket vp(frame);
        std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

        vp.setSyncList(syncList);
        vp.setHeader(hdr);
        boost::shared_ptr<NetworkData> parity = vp.getParityData(VideoFrameSegment::payloadLength(8000), 0.2);

        EXPECT_EQ(hdr.sampleRate_, vp.getHeader().sampleRate_);
        EXPECT_EQ(hdr.publishTimestampMs_, vp.getHeader().publishTimestampMs_);
        EXPECT_EQ(hdr.publishUnixTimestamp_, vp.getHeader().publishUnixTimestamp_);
        EXPECT_EQ(frame._encodedWidth, vp.getFrame()._encodedWidth);
        EXPECT_EQ(frame._encodedHeight, vp.getFrame()._encodedHeight);
    }
}

TEST(TestAudioThreadMeta, TestCreate)
{
    AudioThreadMeta meta(50, 146, "opus");

    EXPECT_TRUE(meta.isValid());
    EXPECT_EQ(50, meta.getRate());
    EXPECT_EQ(146, meta.getBundleNo());
    EXPECT_EQ("opus", meta.getCodec());

    GT_PRINTF("Audio thread meta is %d bytes long\n", meta.getLength());

    NetworkData nd(boost::move(meta));
    EXPECT_FALSE(meta.isValid());

    AudioThreadMeta meta2(boost::move(nd));
    EXPECT_TRUE(meta2.isValid());
    EXPECT_EQ(50, meta2.getRate());
    EXPECT_EQ(146, meta2.getBundleNo());
    EXPECT_EQ("opus", meta2.getCodec());
}

TEST(TestAudioThreadMeta, TestCreateFail)
{
    {
        AudioThreadMeta meta(50, 146, "");

        EXPECT_FALSE(meta.isValid());

        NetworkData nd(boost::move(meta));
        AudioThreadMeta meta2(boost::move(nd));

        EXPECT_FALSE(meta2.isValid());
    }
    {
        uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                                0x39, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
        std::size_t const data_len = sizeof(data) / sizeof(data[0]);
        CommonSamplePacket sp(data_len, data);
        CommonHeader hdr = {25, 39936287, 1460399362};
        sp.setHeader(hdr);

        AudioThreadMeta meta(boost::move(sp));
        EXPECT_FALSE(meta.isValid());
    }
}

TEST(TestVideoThreadMeta, TestCreate)
{
    FrameSegmentsInfo segInfo({5.6, 2.3, 54.3, 12.3});
    VideoCoderParams coder = sampleVideoCoderParams();
    VideoThreadMeta meta(27, 465, 15, 14, segInfo, coder);

    EXPECT_TRUE(meta.isValid());
    EXPECT_EQ(27, meta.getRate());
    EXPECT_EQ(465, meta.getSeqNo().first);
    EXPECT_EQ(15, meta.getSeqNo().second);
    EXPECT_EQ(14, meta.getGopPos());
    EXPECT_EQ(segInfo, meta.getSegInfo());
    {
        VideoCoderParams c = meta.getCoderParams();
        EXPECT_EQ(coder.gop_, c.gop_);
        EXPECT_EQ(coder.encodeWidth_, c.encodeWidth_);
        EXPECT_EQ(coder.encodeHeight_, c.encodeHeight_);
        EXPECT_EQ(coder.startBitrate_, c.startBitrate_);
    }
    GT_PRINTF("Video thread meta is %d bytes long\n", meta.getLength());

    NetworkData nd(boost::move(meta));
    VideoThreadMeta meta2(boost::move(nd));

    EXPECT_TRUE(meta2.isValid());
    EXPECT_EQ(27, meta2.getRate());
    EXPECT_EQ(465, meta2.getSeqNo().first);
    EXPECT_EQ(15, meta2.getSeqNo().second);
    EXPECT_EQ(14, meta2.getGopPos());
    EXPECT_EQ(segInfo, meta2.getSegInfo());
    {
        VideoCoderParams c = meta.getCoderParams();
        EXPECT_EQ(coder.gop_, c.gop_);
        EXPECT_EQ(coder.encodeWidth_, c.encodeWidth_);
        EXPECT_EQ(coder.encodeHeight_, c.encodeHeight_);
        EXPECT_EQ(coder.startBitrate_, c.startBitrate_);
    }
}

TEST(TestVideoThreadMeta, TestCreateFail)
{
    uint8_t const data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                            0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48};
    std::size_t const data_len = sizeof(data) / sizeof(data[0]);
    CommonSamplePacket sp(data_len, data);
    CommonHeader hdr = {25, 39936287, 1460399362};
    sp.setHeader(hdr);

    VideoThreadMeta meta(boost::move(sp));
    EXPECT_FALSE(meta.isValid());
}

TEST(TestMediaStreamMeta, TestCreate)
{
    MediaStreamMeta meta(1526305815742);
    meta.addThread("low");
    meta.addThread("mid");
    meta.addThread("high");
    meta.addThread("ultra");
    meta.addSyncStream("desktop");
    meta.addSyncStream("mic");

    EXPECT_TRUE(meta.isValid());
    ASSERT_EQ(4, meta.getThreads().size());
    EXPECT_EQ("low", meta.getThreads()[0]);
    EXPECT_EQ("mid", meta.getThreads()[1]);
    EXPECT_EQ("high", meta.getThreads()[2]);
    EXPECT_EQ("ultra", meta.getThreads()[3]);
    EXPECT_EQ(1526305815742, meta.getStreamTimestamp());

    ASSERT_EQ(2, meta.getSyncStreams().size());
    EXPECT_EQ("desktop", meta.getSyncStreams()[0]);
    EXPECT_EQ("mic", meta.getSyncStreams()[1]);

    GT_PRINTF("Media stream meta is %d bytes long\n", meta.getLength());

    NetworkData nd(boost::move(meta));
    MediaStreamMeta meta2(boost::move(nd));

    EXPECT_TRUE(meta2.isValid());
    ASSERT_EQ(4, meta2.getThreads().size());
    EXPECT_EQ("low", meta2.getThreads()[0]);
    EXPECT_EQ("mid", meta2.getThreads()[1]);
    EXPECT_EQ("high", meta2.getThreads()[2]);
    EXPECT_EQ("ultra", meta2.getThreads()[3]);

    ASSERT_EQ(2, meta2.getSyncStreams().size());
    EXPECT_EQ("desktop", meta2.getSyncStreams()[0]);
    EXPECT_EQ("mic", meta2.getSyncStreams()[1]);
    EXPECT_EQ(1526305815742, meta.getStreamTimestamp());
}

TEST(TestMediaStreamMeta, TestPackAndUnpack)
{
    MediaStreamMeta meta(1526305815742);
    meta.addThread("low");
    meta.addSyncStream("mic");

    EXPECT_TRUE(meta.isValid());
    ASSERT_EQ(1, meta.getThreads().size());
    EXPECT_EQ("low", meta.getThreads()[0]);
    EXPECT_EQ(1526305815742, meta.getStreamTimestamp());

    ASSERT_EQ(1, meta.getSyncStreams().size());
    EXPECT_EQ("mic", meta.getSyncStreams()[0]);

    GT_PRINTF("Media stream meta is %d bytes long\n", meta.getLength());

    // HeaderPacket<DataSegmenHeader>
    std::vector<CommonSegment> segs = CommonSegment::slice(meta, 1000);
    ASSERT_EQ(1, segs.size());

    DataSegmentHeader hdr;
    hdr.interestNonce_ = 1;
    hdr.interestArrivalMs_ = 2;
    hdr.generationDelayMs_ = 3;
    segs.front().setHeader(hdr);

    boost::shared_ptr<std::vector<uint8_t>>
        bytes(boost::make_shared<std::vector<uint8_t>>(segs.front().getNetworkData()->data()));
    ImmutableHeaderPacket<DataSegmentHeader> packet(bytes);

    EXPECT_EQ(1, packet.getHeader().interestNonce_);
    EXPECT_EQ(2, packet.getHeader().interestArrivalMs_);
    EXPECT_EQ(3, packet.getHeader().generationDelayMs_);

    NetworkData nd(packet.getPayload().size(), packet.getPayload().data());
    MediaStreamMeta meta2(boost::move(nd));

    EXPECT_TRUE(meta2.isValid());
    ASSERT_EQ(1, meta2.getThreads().size());
    EXPECT_EQ("low", meta2.getThreads()[0]);
    EXPECT_EQ(1526305815742, meta2.getStreamTimestamp());

    ASSERT_EQ(1, meta2.getSyncStreams().size());
    EXPECT_EQ("mic", meta2.getSyncStreams()[0]);
}

TEST(TestWireData, TestVideoFrameSegment)
{
    int data_len = 6472;
    std::vector<uint8_t> data;

    for (int i = 0; i < data_len; ++i)
        data.push_back((uint8_t)i);

    NetworkData nd(data);
    int wireLength = 1000;
    std::vector<VideoFrameSegment> segments = VideoFrameSegment::slice(nd, wireLength);
    std::vector<boost::shared_ptr<ndn::Interest>> interests;
    int payloadLength = VideoFrameSegment::payloadLength(wireLength);

    EXPECT_EQ(VideoFrameSegment::numSlices(nd, wireLength), segments.size());

    int idx = 0;
    VideoFrameSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    header.totalSegmentsNum_ = segments.size();
    header.playbackNo_ = 777;
    header.pairedSequenceNo_ = 1;
    header.paritySegmentsNum_ = 2;

    for (auto &s : segments)
    {
        VideoFrameSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        hdr.interestArrivalMs_ += idx;
        hdr.playbackNo_ += idx;
        idx++;
        s.setHeader(hdr);
    }

    {
        std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/video/camera/hi/d/%FE%00";
        std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
        int segIdx = 0;
        for (auto &s : segments)
        {
            ndn::Name n(frameName);
            n.appendSegment(segIdx++);
            boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
            ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size() - 1));
            ds->setContent(s.getNetworkData()->getData(), s.size());
            dataSegments.push_back(ds);

            interests.push_back(boost::make_shared<ndn::Interest>(ds->getName(), 1000));
            interests.back()->setNonce(ndn::Blob((uint8_t *)&(s.getHeader().interestNonce_), sizeof(int)));
        }

        int idx = 0;
        for (auto d : dataSegments)
        {
            WireData<VideoFrameSegmentHeader> wd(d, interests[idx]);

            EXPECT_TRUE(wd.isValid());
            EXPECT_EQ(segments.size(), wd.getSlicesNum());
            EXPECT_EQ(ndn::Name("/ndn/edu/ucla/remap/ndncon/instance1"), wd.getBasePrefix());
            EXPECT_EQ(NameComponents::nameApiVersion(), wd.getApiVersion());
            EXPECT_EQ(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, wd.getStreamType());
            EXPECT_EQ("camera", wd.getStreamName());
            EXPECT_EQ("hi", wd.getThreadName());
            EXPECT_FALSE(wd.isMeta());
            EXPECT_EQ(0, wd.getSampleNo());
            EXPECT_EQ(idx, wd.getSegNo());
            EXPECT_FALSE(wd.isParity());

            WireSegment *seg = &wd;
            EXPECT_EQ(777 + idx, seg->getPlaybackNo());
            EXPECT_EQ(777 + idx, wd.getPlaybackNo());

            EXPECT_TRUE(wd.isOriginal());
            EXPECT_EQ(header.interestNonce_ + idx, wd.segment().getHeader().interestNonce_);
            EXPECT_EQ(header.interestArrivalMs_ + idx, wd.segment().getHeader().interestArrivalMs_);
            EXPECT_EQ(header.playbackNo_ + idx, wd.segment().getHeader().playbackNo_);
            EXPECT_EQ(header.generationDelayMs_, wd.segment().getHeader().generationDelayMs_);
            EXPECT_EQ(header.totalSegmentsNum_, wd.segment().getHeader().totalSegmentsNum_);
            EXPECT_EQ(header.pairedSequenceNo_, wd.segment().getHeader().pairedSequenceNo_);
            EXPECT_EQ(header.paritySegmentsNum_, wd.segment().getHeader().paritySegmentsNum_);

            EXPECT_EQ(header.interestNonce_ + idx, wd.header().interestNonce_);
            EXPECT_EQ(header.interestArrivalMs_ + idx, wd.header().interestArrivalMs_);
            EXPECT_EQ(header.generationDelayMs_, wd.header().generationDelayMs_);

            idx++;
        }
    }
#if 0
    {
        std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%00/video/camera/hi/d/%FE%00";
        std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
        int segIdx = 0;
        for (auto& s:segments)
        {
            ndn::Name n(frameName);
            n.appendSegment(segIdx++);
            boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
            ds->setContent(s.getNetworkData()->getData(), s.size());
            dataSegments.push_back(ds);
        }
    
        idx = 0;
        for (auto d:dataSegments)
            EXPECT_ANY_THROW(WireData<VideoFrameSegmentHeader> wd(d, interests[idx++]));
    }
#endif
}

TEST(TestWireData, TestWrongData)
{
    int data_len = 1000;

    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/video/camera/hi/d/%FE%00";
    ndn::Name n(frameName);
    n.appendSegment(0);
    boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
    ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(1));

    std::vector<uint8_t> data;
    for (int i = 0; i < data_len; ++i)
        data.push_back(i);

    ds->setContent(data);

    WireData<VideoFrameSegmentHeader> wd(ds, boost::make_shared<ndn::Interest>(n));
    EXPECT_TRUE(wd.isValid());
    EXPECT_FALSE(wd.segment().isValid());
}

TEST(TestWireData, TestWrongHeader)
{
    int data_len = 6472;
    std::vector<uint8_t> data;

    for (int i = 0; i < data_len; ++i)
        data.push_back((uint8_t)i);

    NetworkData nd(data);
    int wireLength = 1000;
    std::vector<CommonSegment> segments = CommonSegment::slice(nd, wireLength);

    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/video/camera/hi/d/%FE%00";
    ndn::Name n(frameName);
    n.appendSegment(0);
    boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
    boost::shared_ptr<ndn::Interest> is(boost::make_shared<ndn::Interest>(n));
    ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(1));

    DataSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    segments.front().setHeader(header);
    ds->setContent(segments.front().getNetworkData()->getData(), segments.front().size());
    is->setNonce(ndn::Blob((uint8_t *)&(header.interestNonce_), sizeof(int)));

    {
        WireData<VideoFrameSegmentHeader> wd(ds, is);
        EXPECT_TRUE(wd.isValid());
        EXPECT_FALSE(wd.segment().isValid());
        EXPECT_TRUE(wd.isOriginal());

        EXPECT_EQ(header.interestNonce_, wd.header().interestNonce_);
        EXPECT_EQ(header.interestArrivalMs_, wd.header().interestArrivalMs_);
        EXPECT_EQ(header.generationDelayMs_, wd.header().generationDelayMs_);
    }

    {
        WireData<DataSegmentHeader> wd(ds, is);
        EXPECT_TRUE(wd.isValid());
        EXPECT_TRUE(wd.segment().isValid());
        EXPECT_TRUE(wd.isOriginal());

        EXPECT_EQ(header.interestNonce_, wd.header().interestNonce_);
        EXPECT_EQ(header.interestArrivalMs_, wd.header().interestArrivalMs_);
        EXPECT_EQ(header.generationDelayMs_, wd.header().generationDelayMs_);
    }
}

TEST(TestWireData, TestMergeVideoFramePacket)
{
    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kVideoFrameKey;
    frame._completeFrame = true;

    VideoFramePacket vp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    vp.setSyncList(syncList);
    vp.setHeader(hdr);
    boost::shared_ptr<NetworkData> parity = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

    std::vector<VideoFrameSegment> frameSegments = VideoFrameSegment::slice(vp, 1000);
    std::vector<CommonSegment> paritySegments = CommonSegment::slice(*parity, 1000);

    // pack segments into data objects
    int idx = 0;
    VideoFrameSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    header.totalSegmentsNum_ = frameSegments.size();
    header.playbackNo_ = 0;
    header.pairedSequenceNo_ = 1;
    header.paritySegmentsNum_ = 2;

    for (auto &s : frameSegments)
    {
        VideoFrameSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        hdr.interestArrivalMs_ += idx;
        hdr.playbackNo_ += idx;
        idx++;
        s.setHeader(hdr);
    }

    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/video/camera/hi/d/%FE%00";
    std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
    std::vector<boost::shared_ptr<ndn::Interest>> interests;
    int segIdx = 0;
    for (auto &s : frameSegments)
    {
        ndn::Name n(frameName);
        n.appendSegment(segIdx++);
        boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
        ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(frameSegments.size()));
        ds->setContent(s.getNetworkData()->getData(), s.size());
        dataSegments.push_back(ds);

        interests.push_back(boost::make_shared<ndn::Interest>(n, 1000));
        interests.back()->setNonce(ndn::Blob((uint8_t *)&(s.getHeader().interestNonce_), sizeof(int)));
    }

    idx = 0;
    // now, extract segments from data objects
    std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> videoSegments;
    for (auto d : dataSegments)
    {
        WireData<VideoFrameSegmentHeader> wd(d, interests[idx++]);

        if (wd.getSegNo() == 0)
        {
            EXPECT_EQ(hdr.sampleRate_, wd.packetHeader().sampleRate_);
            EXPECT_EQ(hdr.publishTimestampMs_, wd.packetHeader().publishTimestampMs_);
            EXPECT_EQ(hdr.publishUnixTimestamp_, wd.packetHeader().publishUnixTimestamp_);
        }
        else
            EXPECT_ANY_THROW(wd.packetHeader());

        videoSegments.push_back(wd.segment());
    }

    // merge video segments
    boost::shared_ptr<VideoFramePacket> packet = VideoFramePacket::merge(videoSegments);

    EXPECT_EQ(hdr.sampleRate_, packet->getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, packet->getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, packet->getHeader().publishUnixTimestamp_);

    EXPECT_EQ(frame._encodedWidth, packet->getFrame()._encodedWidth);
    EXPECT_EQ(frame._encodedHeight, packet->getFrame()._encodedHeight);
    EXPECT_EQ(frame._timeStamp, packet->getFrame()._timeStamp);
    EXPECT_EQ(frame.capture_time_ms_, packet->getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType, packet->getFrame()._frameType);
    EXPECT_EQ(frame._completeFrame, packet->getFrame()._completeFrame);
    ASSERT_EQ(frame._length, packet->getFrame()._length);

    bool identical = true;
    for (int i = 0; i < packet->getFrame()._length && identical; ++i)
        identical = (frame._buffer[i] == packet->getFrame()._buffer[i]);
    EXPECT_TRUE(identical);

    EXPECT_EQ(syncList.size(), packet->getSyncList().size());
    idx = 0;
    for (auto t : syncList)
    {
        ASSERT_NE(packet->getSyncList().end(), packet->getSyncList().find(t.first));
        EXPECT_EQ(t.second, packet->getSyncList().at(t.first));
    }
}

TEST(TestWireData, TestMergeAudioBundle)
{
    int data_len = 247;
    std::vector<uint8_t> rtpData;
    for (int i = 0; i < data_len; ++i)
        rtpData.push_back((uint8_t)i);

    int wire_len = 1000;
    AudioBundlePacket bundlePacket(wire_len);
    AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestamp_ = 1460488589;

    bundlePacket.setHeader(hdr);

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundlePacket.getSamplesNum());

    std::vector<CommonSegment> segments = CommonSegment::slice(bundlePacket, wire_len);
    ASSERT_EQ(1, segments.size());

    DataSegmentHeader shdr;
    shdr.interestNonce_ = 0x1234;
    shdr.interestArrivalMs_ = 1460399362;
    shdr.generationDelayMs_ = 200;
    segments[0].setHeader(shdr);

    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/audio/mic/hd/%FE%07";
    ndn::Name n(frameName);
    n.appendSegment(0);
    boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
    ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(0));
    ds->setContent(segments.front().getNetworkData()->data());

    boost::shared_ptr<ndn::Interest> interest = boost::make_shared<ndn::Interest>(n, 1000);
    interest->setNonce(ndn::Blob((uint8_t *)&(shdr.interestNonce_), sizeof(int)));

    WireData<DataSegmentHeader> wd(ds, interest);
    EXPECT_TRUE(wd.isValid());
    EXPECT_TRUE(wd.segment().isValid());
    EXPECT_EQ(1, wd.getSlicesNum());
    EXPECT_EQ(hdr.sampleRate_, wd.packetHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, wd.packetHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestamp_, wd.packetHeader().publishUnixTimestamp_);

    WireSegment *seg = &wd;
    EXPECT_EQ(7, seg->getPlaybackNo());
    EXPECT_EQ(7, wd.getPlaybackNo());

    std::vector<ImmutableHeaderPacket<DataSegmentHeader>> bundleSegments;
    bundleSegments.push_back(wd.segment());

    boost::shared_ptr<AudioBundlePacket> bundleP = AudioBundlePacket::merge(bundleSegments);

    // check
    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len) / AudioBundlePacket::AudioSampleBlob::wireLength(data_len),
              bundleP->getSamplesNum());
    for (int i = 0; i < bundleP->getSamplesNum(); ++i)
    {
        EXPECT_FALSE((*bundleP)[i].getHeader().isRtcp_);
        bool identical = true;
        for (int k = 0; k < (*bundleP)[i].size() - sizeof(AudioSampleHeader); ++k)
            identical = (rtpData[k], (*bundleP)[i].data()[k]);
        EXPECT_TRUE(identical);
    }
}

TEST(TestManifest, TestWithDummySignature)
{
    std::string frameName = "/ndn/edu/ucla/remap/ndncon/instance1/ndnrtc/%FD%03/audio/mic/hd/%FE%07";
    size_t frameSize = 60000;
    VideoFramePacket vp = getVideoFramePacket(frameSize);
    std::vector<VideoFrameSegment> segments = sliceFrame(vp);
    boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    std::vector<VideoFrameSegment> paritySegments = sliceParity(vp, parityData);
    std::vector<boost::shared_ptr<ndn::Data>> dataObjects = dataFromSegments(frameName, segments);
    std::vector<boost::shared_ptr<ndn::Data>> parityObjects = dataFromParitySegments(frameName, paritySegments);
    std::vector<boost::shared_ptr<ndn::Data>> allObjects(dataObjects);

    std::copy(parityObjects.begin(), parityObjects.end(), std::back_inserter(allObjects));

    std::vector<boost::shared_ptr<const ndn::Data>> allSegments;

    for (auto &o : allObjects)
    {
        static uint8_t digest[ndn_SHA256_DIGEST_SIZE];
        memset(digest, 0, ndn_SHA256_DIGEST_SIZE);
        ndn::Blob signatureBits(digest, sizeof(digest));
        o->setSignature(ndn::DigestSha256Signature());
        ndn::DigestSha256Signature *sha256Signature = (ndn::DigestSha256Signature *)o->getSignature();
        sha256Signature->setSignature(signatureBits);

        ndn::Name::Component c = (*(o->getFullName()))[-1];
        allSegments.push_back(boost::shared_ptr<const ndn::Data>(o));
    }

    Manifest m(allSegments);

    EXPECT_EQ(allSegments.size(), m.size());

    for (auto &o : allObjects)
        EXPECT_TRUE(m.hasData(*o));

    GT_PRINTF("Manifest packet of %d segments (frame size %d bytes) has total length of %d bytes\n",
              m.size(), frameSize, m.getLength());

    NetworkData nd(m);
    Manifest im(boost::move(nd));

    EXPECT_EQ(allSegments.size(), im.size());

    for (auto &o : allObjects)
        EXPECT_TRUE(im.hasData(*o));
}

//******************************************************************************
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
