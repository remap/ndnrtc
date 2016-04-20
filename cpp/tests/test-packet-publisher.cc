// 
// test-packet-publisher.cc
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <boost/assign.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.h"
#include "src/packet-publisher.h"
#include "mock-objects/ndn-cpp-mock.h"
#include "frame-data.h"

// include .cpp in order to instantiate class with mock objects
#include "src/packet-publisher.cpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndn;

typedef _PublisherSettings<MockNdnKeyChain, MockNdnMemoryCache> MockSettings;
typedef std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest> > PendingInterests;

TEST(TestPacketPublisher, TestPublishVideoFrame)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		dataObjects.push_back(data);
		// check CRC value
		NetworkData nd(*data.getContent());
		uint64_t crcValue = data.getName()[-1].toNumber();
		EXPECT_EQ(crcValue, nd.getCrcValue());
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
		.Times(1)
		.WillOnce(ReturnRef(mockCallback));
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(1);
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<VideoFrameSegment, MockSettings> publisher(settings, filter);

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

		{ // test one pending interest
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test two pending interests
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			dataObjects.clear();
			boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest2 = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
			pendingInterests.push_back(pendingInterest2);

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest2->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest2->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test no pending interests
			dataObjects.clear();
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			pendingInterests.clear(); 

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_EQ(0, segHdr.generationDelayMs_);
			EXPECT_EQ(0, segHdr.interestArrivalMs_);
			EXPECT_EQ(0, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
	}
}

TEST(TestPacketPublisher, TestPublishVideoParity)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;

	Name filter("/testpacket");
	Name packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		EXPECT_EQ(CommonSegment::wireLength(VideoFrameSegment::payloadLength(1000)), data.getContent().size());
		dataObjects.push_back(data);
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
		.Times(0);
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(0); // no filter provided
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<CommonSegment, MockSettings> publisher(settings);

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

		{ // test one pending interest
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			DataSegmentHeader segHdr;

			size_t nSlices = publisher.publish(packetName, *parityData, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
	}
}

TEST(TestPacketPublisher, TestPublishAudioBundle)
{
		Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	int data_len = 247;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness, data_len](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		EXPECT_EQ(CommonSegment::wireLength(AudioBundlePacket::wireLength(1000, data_len)), data.getContent().size());
		dataObjects.push_back(data);
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
		.Times(1)
		.WillOnce(ReturnRef(mockCallback));
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(1);
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<CommonSegment, MockSettings> publisher(settings, filter);

	{
		
		std::vector<uint8_t> rtpData;
		for (int i = 0; i < data_len; ++i)
			rtpData.push_back((uint8_t)i);

		int wire_len = wireLength;
		AudioBundlePacket bundlePacket(wireLength);
		AudioBundlePacket::AudioSampleBlob sample({false}, data_len, rtpData.data());

		while (bundlePacket.hasSpace(sample)) bundlePacket << sample;
		
		CommonHeader hdr;
		hdr.sampleRate_ = 24.7;
		hdr.publishTimestampMs_ = 488589553;
		hdr.publishUnixTimestampMs_ = 1460488589;

		bundlePacket.setHeader(hdr);

		DataSegmentHeader segHdr;
		size_t nSlices = publisher.publish(packetName, bundlePacket, segHdr);
		EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
		EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
		EXPECT_EQ(nonce, segHdr.interestNonce_);
		EXPECT_EQ(nSlices, dataObjects.size());
		GT_PRINTF("Targeting %d wire length segment, audio bundle wire length is %d bytes "
			"(for sample size %d, bundle fits %d samples total)\n", wire_len, bundlePacket.getLength(), 
			data_len, bundlePacket.getSamplesNum());
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
