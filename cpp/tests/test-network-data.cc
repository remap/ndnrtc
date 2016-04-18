// 
// test-network-data.cc
//
//  Created by Peter Gusev on 09 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/move/move.hpp>
#include <boost/assign.hpp>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "tests-helpers.h"
#include "gtest/gtest.h"
#include "src/frame-data.h"

using namespace ndnrtc;

class DataPacketTest : public DataPacket
{
public:
    DataPacketTest(unsigned int dataLength, const uint8_t* payload):
        DataPacket(dataLength, payload){}
    DataPacketTest(const std::vector<uint8_t>& payload):DataPacket(payload){}
    DataPacketTest(const DataPacket& dataPacket):DataPacket(dataPacket){}
    DataPacketTest(NetworkData&& networkData):DataPacket(boost::move(networkData)){}

    unsigned int getBlobsNum() const { return DataPacket::getBlobsNum(); }
    const Blob getBlob(size_t pos) const { return DataPacket::getBlob(pos); }
    void addBlob(uint16_t dataLength, const uint8_t* data) { DataPacket::addBlob(dataLength, data); }
};

TEST(TestNetworkData, TestCreate1)
{
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;

    NetworkData nd(data_len, data);

    EXPECT_EQ(data_len, nd.getLength());
    for (int i = 0; i < nd.getLength(); ++i)
    	EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());
}

TEST(TestNetworkData, TestCreate2)
{
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00};
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    std::size_t const sensible_data_len = data_len - 1;
    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;
    std::vector<uint8_t> v(&data[0], &data[data_len-1]);

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
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;

    NetworkData nd(data_len, data);
    NetworkData nd2(nd);

    EXPECT_EQ(nd.getLength(), nd2.getLength());
    for (int i = 0; i < nd2.getLength(); ++i)
    	EXPECT_EQ(nd.getData()[i], nd2.getData()[i]);
    EXPECT_EQ(crcExpected, nd2.getCrcValue());
}

TEST(TestNetworkData, TestMove)
{
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;

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
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    uint8_t const  data2[] = { 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len2 = sizeof( data2 ) / sizeof( data2[0] );

    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;
    boost::uint16_t const  crcExpected2 = 0x2A64;

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
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    uint8_t const  data2[] = { 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len2 = sizeof( data2 ) / sizeof( data2[0] );

    // The expected CRC for the given data
    boost::uint16_t const  crcExpected = 0xBB3D;
    boost::uint16_t const  crcExpected2 = 0x2A64;

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
	uint8_t const  data[] = { 0x31 };
    std::size_t const data_len = 1;
    boost::uint16_t const  crcExpected = 0xD4C1;

    NetworkData nd(NetworkData(data_len, data));
    EXPECT_EQ(data_len, nd.getLength());
	for (int i = 0; i < nd.getLength(); ++i)
    	EXPECT_EQ(data[i], nd.getData()[i]);
    EXPECT_EQ(crcExpected, nd.getCrcValue());    
}

TEST(TestDataPacket, TestCreate)
{
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );

	DataPacketTest dp(data_len, data);

	EXPECT_EQ(data_len+1, dp.getLength());
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
        dp.addBlob(sizeof(number), (uint8_t*)&number);
        EXPECT_EQ(1, dp.getBlobsNum());
        EXPECT_EQ(number, *(int*)dp.getBlob(0).data());
        EXPECT_TRUE(dp.isValid());
    }
}

TEST(TestDataPacket, TestBlobs)
{
	uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    uint8_t const  header1[] = { 0x31 };
    std::size_t const header1_data_len = sizeof( header1 ) / sizeof( header1[0] );
    uint8_t const  header2[] = { 0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const header2_data_len = sizeof( header2 ) / sizeof( header2[0] );
    uint8_t const  header3[] = { 0x31, 0x32, 0x33, 0x34 };
    std::size_t const header3_data_len = sizeof( header3 ) / sizeof( header3[0] );

	DataPacketTest dp(data_len, data);

	dp.addBlob(header1_data_len, header1);
	dp.addBlob(header2_data_len, header2);
	dp.addBlob(header3_data_len, header3);

	int x = 5;
	dp.addBlob(sizeof(int), (const uint8_t*)&x);

	ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
	EXPECT_EQ(data_len+header1_data_len+header2_data_len+header3_data_len+sizeof(int)+4*sizeof(uint16_t)+1, dp.getLength());
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
	int y = *((int*)dp.getBlob(3).data());
	EXPECT_EQ(x, y);
    std::vector<size_t> blobLengths = boost::assign::list_of(header1_data_len)(header2_data_len)(header3_data_len)(sizeof(int));
    EXPECT_EQ(DataPacket::wireLength(data_len, blobLengths), dp.getLength());
}

TEST(TestDataPacket, TestDataPacketCopy)
{
    uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    uint8_t const  header1[] = { 0x31 };
    std::size_t const header1_data_len = sizeof( header1 ) / sizeof( header1[0] );
    uint8_t const  header2[] = { 0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const header2_data_len = sizeof( header2 ) / sizeof( header2[0] );
    uint8_t const  header3[] = { 0x31, 0x32, 0x33, 0x34 };
    std::size_t const header3_data_len = sizeof( header3 ) / sizeof( header3[0] );

    DataPacketTest dp0(data_len, data);

    dp0.addBlob(header1_data_len, header1);
    EXPECT_TRUE(dp0.isValid());
    dp0.addBlob(header2_data_len, header2);
    EXPECT_TRUE(dp0.isValid());
    dp0.addBlob(header3_data_len, header3);
    EXPECT_TRUE(dp0.isValid());

    int x = 5;
    dp0.addBlob(sizeof(int), (const uint8_t*)&x);
    EXPECT_TRUE(dp0.isValid());

    DataPacketTest dp(dp0);

    ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(data_len+header1_data_len+header2_data_len+header3_data_len+sizeof(int)+4*sizeof(uint16_t)+1, dp.getLength());
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
    int y = *((int*)dp.getBlob(3).data());
    EXPECT_EQ(x, y);
}

TEST(TestDataPacket, TestDataPacketAssign)
{
    uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    uint8_t const  header1[] = { 0x31 };
    std::size_t const header1_data_len = sizeof( header1 ) / sizeof( header1[0] );
    uint8_t const  header2[] = { 0x30, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const header2_data_len = sizeof( header2 ) / sizeof( header2[0] );
    uint8_t const  header3[] = { 0x31, 0x32, 0x33, 0x34 };
    std::size_t const header3_data_len = sizeof( header3 ) / sizeof( header3[0] );

    DataPacketTest dp0(data_len, data);

    dp0.addBlob(header1_data_len, header1);
    dp0.addBlob(header2_data_len, header2);
    dp0.addBlob(header3_data_len, header3);

    int x = 5;
    dp0.addBlob(sizeof(int), (const uint8_t*)&x);

    DataPacketTest dp = dp0;

    ASSERT_EQ(4, dp.getBlobsNum());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(data_len+header1_data_len+header2_data_len+header3_data_len+sizeof(int)+4*sizeof(uint16_t)+1, dp.getLength());
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
    int y = *((int*)dp.getBlob(3).data());
    EXPECT_EQ(x, y);
}

TEST(TestDataPacket, TestMoveNetworkData)
{
    uint8_t const  data[] = { 0x01, 0x04, 0x00, 't', 'e', 's', 't',
     0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );

    NetworkData nd(data_len, data);
    DataPacketTest dp(boost::move(nd));

    EXPECT_EQ(0, nd.getLength());
    EXPECT_EQ(nullptr, nd.getData());
    EXPECT_TRUE(dp.isValid());
    EXPECT_EQ(1, dp.getBlobsNum());
    EXPECT_EQ("test", std::string((char*)dp.getBlob(0).data(), dp.getBlob(0).size()));
    EXPECT_EQ("123456789", std::string((char*)dp.getPayload().data(), dp.getPayload().size()));
}

TEST(TestDataPacket, TestCreateEmptyPacket)
{
    EXPECT_EQ(1, DataPacket::wireLength(0,0));
    std::vector<size_t> v = boost::assign::list_of (0);
    EXPECT_EQ(1, DataPacket::wireLength(0, v));

    DataPacket dp(boost::move(NetworkData(std::vector<uint8_t>())));
    EXPECT_FALSE(dp.isValid());
}

TEST(TestDataPacket, TestInvalidRawData)
{
    uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
    NetworkData nd(data_len, data);
    DataPacketTest dp(boost::move(nd));

    EXPECT_FALSE(dp.isValid());
}

TEST(TestSamplePacket, TestCreate)
{
    {
        uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
        std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
        CommonSamplePacket sp(data_len, data);
        CommonHeader hdr = {25, 39936287, 1460399362};
        
        EXPECT_FALSE(sp.isValid());
        
        sp.setHeader(hdr);

        EXPECT_TRUE(sp.isValid());
        EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
        EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
        EXPECT_EQ(hdr.publishUnixTimestampMs_, sp.getHeader().publishUnixTimestampMs_);

        EXPECT_ANY_THROW(sp.setHeader(hdr));
    }
    {
        uint8_t const  data[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
        std::size_t const data_len = sizeof( data ) / sizeof( data[0] );
        CommonHeader hdr = {25, 39936287, 1460399362};
        CommonSamplePacket sp(hdr, data_len, data);

        EXPECT_TRUE(sp.isValid());
        EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
        EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
        EXPECT_EQ(hdr.publishUnixTimestampMs_, sp.getHeader().publishUnixTimestampMs_);

        EXPECT_ANY_THROW(sp.setHeader(hdr));
    }
}

TEST(TestSamplePacket, TestCreateFromRaw)
{
    CommonHeader hdr = {25, 39936287, 1460399362};
    uint8_t const  payload[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const payload_len = sizeof( payload ) / sizeof( payload[0] );
    std::vector<uint8_t> data(&payload[0], &payload[0]+payload_len);
    data.insert(data.begin(), (uint8_t*)&hdr, (uint8_t*)&hdr+sizeof(CommonHeader));
    uint16_t sz = sizeof(CommonHeader);
    data.insert(data.begin(), (sz&0xff00)>>8);
    data.insert(data.begin(), (sz&0x00ff));
    data.insert(data.begin(), 1);
    NetworkData nd(data);
    CommonSamplePacket sp(boost::move(nd));

    ASSERT_TRUE(sp.isValid());
    EXPECT_EQ(hdr.sampleRate_, sp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, sp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, sp.getHeader().publishUnixTimestampMs_);
    EXPECT_EQ(payload_len, sp.getPayload().size());
    for (int i = 0; i < payload_len; i++)
        EXPECT_EQ(payload[i], sp.getPayload()[i]);
}

TEST(TestSamplePacket, TestCreateFromRawInvalid)
{
    uint8_t const  payload[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    std::size_t const payload_len = sizeof( payload ) / sizeof( payload[0] );
    std::vector<uint8_t> data(&payload[0], &payload[0]+payload_len);
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
    
    AudioBundlePacket::AudioSampleBlob sample({false}, data_len, rtpData.data());

    EXPECT_FALSE(sample.getHeader().isRtcp_);
    // EXPECT_EQ(1234, sample.getHeader().dummy_);
    ASSERT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), sample.size());
    ASSERT_EQ(data_len+sizeof(AudioSampleHeader), sample.size());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len)/AudioBundlePacket::AudioSampleBlob::wireLength(data_len), 
        bundlePacket.getSamplesNum());
    for (int i = 0; i < bundlePacket.getSamplesNum(); ++i)
    {
        EXPECT_FALSE(bundlePacket[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, bundlePacket[i].getHeader().dummy_);
        for (int k = 0; k < bundlePacket[i].size()-sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], bundlePacket[i].data()[k]);
    }

    bundlePacket.clear();
    EXPECT_EQ(0, bundlePacket.getSamplesNum());

    while (bundlePacket.hasSpace(sample))
        bundlePacket << sample;

    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len)/AudioBundlePacket::AudioSampleBlob::wireLength(data_len), 
        bundlePacket.getSamplesNum());
    ASSERT_FALSE(bundlePacket.isValid());
    bundlePacket.setHeader({25, 1, 2});
    ASSERT_TRUE(bundlePacket.isValid());
    EXPECT_GE(wire_len, bundlePacket.getLength());
    EXPECT_EQ(AudioBundlePacket::wireLength(wire_len, data_len), bundlePacket.getLength());

    NetworkData nd(boost::move(bundlePacket));
    AudioBundlePacket newBundle(boost::move(nd));
    AudioBundlePacket thirdBundle(wire_len);

    ASSERT_FALSE(bundlePacket.isValid());
    ASSERT_EQ(AudioBundlePacket::wireLength(wire_len, data_len)/AudioBundlePacket::AudioSampleBlob::wireLength(data_len), 
        newBundle.getSamplesNum());
    EXPECT_EQ(25, newBundle.getHeader().sampleRate_);
    EXPECT_EQ(1, newBundle.getHeader().publishTimestampMs_);
    EXPECT_EQ(2, newBundle.getHeader().publishUnixTimestampMs_);

    for (int i = 0; i < newBundle.getSamplesNum(); ++i)
    {
        EXPECT_EQ(AudioBundlePacket::AudioSampleBlob::wireLength(data_len), newBundle[i].size());
        EXPECT_FALSE(newBundle[i].getHeader().isRtcp_);
        // EXPECT_EQ(1234, newBundle[i].getHeader().dummy_);
        for (int k = 0; k < newBundle[i].size()-sizeof(AudioSampleHeader); ++k)
            EXPECT_EQ(rtpData[k], newBundle[i].data()[k]);

        thirdBundle << newBundle[i];
    }

    thirdBundle.setHeader(newBundle.getHeader());
    for (int i = 0; i < thirdBundle.getLength(); ++i)
        EXPECT_EQ(newBundle.getData()[i], thirdBundle.getData()[i]);
}

TEST(TestDataSegment, TestSlice)
{
    int data_len = 6472;
    std::vector<uint8_t> data;

    for (int i = 0; i < data_len; ++i)
        data.push_back((uint8_t)i);
    
    NetworkData nd(data);
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

    for (auto& s:segments)
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
        size_t segIdx = i/payloadLength;
        VideoFrameSegment s = segments[segIdx];
        size_t pos = i%payloadLength;
        EXPECT_EQ(data[i], s.getPayload()[pos]);
    }

    idx = 0;
    size_t lastSegmentPayloadSize = data_len - (data_len/payloadLength)*payloadLength;
    for (std::vector<VideoFrameSegment>::iterator it = segments.begin(); it < segments.end(); ++it)
    {
        if (it+1 == segments.end())
        {
            EXPECT_EQ(VideoFrameSegment::wireLength(lastSegmentPayloadSize), it->getNetworkData()->getLength());
            EXPECT_EQ(VideoFrameSegment::wireLength(lastSegmentPayloadSize), it->size());
        }
        else
        {
            EXPECT_EQ(wireLength, it->getNetworkData()->getLength());
            EXPECT_EQ(wireLength, it->size());
        }

        EXPECT_EQ(header.interestNonce_+idx, it->getHeader().interestNonce_);
        EXPECT_EQ(header.interestArrivalMs_+idx, it->getHeader().interestArrivalMs_);
        EXPECT_EQ(header.generationDelayMs_, it->getHeader().generationDelayMs_);
        EXPECT_EQ(header.totalSegmentsNum_, it->getHeader().totalSegmentsNum_);
        EXPECT_EQ(header.playbackNo_+idx, it->getHeader().playbackNo_);
        EXPECT_EQ(header.pairedSequenceNo_, it->getHeader().pairedSequenceNo_);
        EXPECT_EQ(header.paritySegmentsNum_, it->getHeader().paritySegmentsNum_);
        idx++;
    }
}

TEST(TestVideoFramePacket, TestCreate)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kKeyFrame;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);
    EXPECT_EQ(frame._encodedWidth       , fp.getFrame()._encodedWidth   );
    EXPECT_EQ(frame._encodedHeight      , fp.getFrame()._encodedHeight  );
    EXPECT_EQ(frame._timeStamp          , fp.getFrame()._timeStamp      );
    EXPECT_EQ(frame.capture_time_ms_    , fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType          , fp.getFrame()._frameType      );
    EXPECT_EQ(frame._completeFrame      , fp.getFrame()._completeFrame  );

    GT_PRINTF("frame packet length is %d\n", fp.getLength());

    EXPECT_ANY_THROW(fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2));

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestampMs_ = 1460488589;

    fp.setHeader(hdr);
    ASSERT_TRUE(fp.isValid());

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, fp.getHeader().publishUnixTimestampMs_);

    int length = fp.getLength();
    boost::shared_ptr<NetworkData> parityData = fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    EXPECT_TRUE(parityData.get());
    EXPECT_EQ(length, fp.getLength());
    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, fp.getHeader().publishUnixTimestampMs_);
    EXPECT_EQ(frame._encodedWidth       , fp.getFrame()._encodedWidth   );
    EXPECT_EQ(frame._encodedHeight      , fp.getFrame()._encodedHeight  );
    EXPECT_EQ(frame._timeStamp          , fp.getFrame()._timeStamp      );
    EXPECT_EQ(frame.capture_time_ms_    , fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType          , fp.getFrame()._frameType      );
    EXPECT_EQ(frame._completeFrame      , fp.getFrame()._completeFrame  );
    for (int i = 0; i < frameLen; ++i) EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
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
    uint8_t *buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kKeyFrame;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

    fp.setSyncList(syncList);
    EXPECT_EQ(syncList, fp.getSyncList());

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestampMs_ = 1460488589;

    fp.setHeader(hdr);
    EXPECT_EQ(syncList, fp.getSyncList());    

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, fp.getHeader().publishUnixTimestampMs_);

    int length = fp.getLength();
    boost::shared_ptr<NetworkData> parityData = fp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    EXPECT_TRUE(parityData.get());
    EXPECT_EQ(length, fp.getLength());
    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, fp.getHeader().publishUnixTimestampMs_);
    EXPECT_EQ(frame._encodedWidth       , fp.getFrame()._encodedWidth   );
    EXPECT_EQ(frame._encodedHeight      , fp.getFrame()._encodedHeight  );
    EXPECT_EQ(frame._timeStamp          , fp.getFrame()._timeStamp      );
    EXPECT_EQ(frame.capture_time_ms_    , fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType          , fp.getFrame()._frameType      );
    EXPECT_EQ(frame._completeFrame      , fp.getFrame()._completeFrame  );
    for (int i = 0; i < frameLen; ++i) EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
}

TEST(TestVideoFramePacket, TestFromNetworkData)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kKeyFrame;
    frame._completeFrame = true;

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestampMs_ = 1460488589;

    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

    VideoFramePacket first(frame);    
    first.setSyncList(syncList);
    first.setHeader(hdr);

    VideoFramePacket fp(boost::move((NetworkData&)first));

    EXPECT_EQ(0, first.getLength());
    EXPECT_FALSE(first.isValid());
    EXPECT_EQ(syncList, fp.getSyncList());    

    EXPECT_EQ(hdr.sampleRate_, fp.getHeader().sampleRate_);
    EXPECT_EQ(hdr.publishTimestampMs_, fp.getHeader().publishTimestampMs_);
    EXPECT_EQ(hdr.publishUnixTimestampMs_, fp.getHeader().publishUnixTimestampMs_);
    EXPECT_EQ(frame._encodedWidth       , fp.getFrame()._encodedWidth   );
    EXPECT_EQ(frame._encodedHeight      , fp.getFrame()._encodedHeight  );
    EXPECT_EQ(frame._timeStamp          , fp.getFrame()._timeStamp      );
    EXPECT_EQ(frame.capture_time_ms_    , fp.getFrame().capture_time_ms_);
    EXPECT_EQ(frame._frameType          , fp.getFrame()._frameType      );
    EXPECT_EQ(frame._completeFrame      , fp.getFrame()._completeFrame  );
    for (int i = 0; i < frameLen; ++i) EXPECT_EQ(buffer[i], fp.getFrame()._buffer[i]);
}

TEST(TestVideoFramePacket, TestAddSyncListThrow)
{
    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kKeyFrame;
    frame._completeFrame = true;

    VideoFramePacket fp(frame);

    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestampMs_ = 1460488589;

    fp.setHeader(hdr);

    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

    EXPECT_ANY_THROW(fp.setSyncList(syncList));
}

TEST(TestVideoFramePacket, TestSliceFrame)
{
    CommonHeader hdr;
    hdr.sampleRate_ = 24.7;
    hdr.publishTimestampMs_ = 488589553;
    hdr.publishUnixTimestampMs_ = 1460488589;

    size_t frameLen = 4300;
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    uint8_t *buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = webrtc::kKeyFrame;
    frame._completeFrame = true;

    VideoFramePacket vp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

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

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}