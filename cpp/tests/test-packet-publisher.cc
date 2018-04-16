//
// test-packet-publisher.cc
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <boost/assign.hpp>
#include <boost/asio.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "src/packet-publisher.hpp"
#include "mock-objects/ndn-cpp-mock.hpp"
#include "frame-data.hpp"

// include .cpp in order to instantiate class with mock objects
#include "src/packet-publisher.cpp"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

typedef _PublisherSettings<MockNdnKeyChain, MockNdnMemoryCache> MockSettings;
typedef std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> PendingInterests;

TEST(TestPacketPublisher, TestPublishVideoFrame)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest =
        boost::make_shared<Interest>("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d/%FE%00", 2000);
    boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest =
        boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
    PendingInterests pendingInterests;

    uint32_t nonce = 1234;
    interest->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));

    MockNdnKeyChain keyChain;
    MockNdnMemoryCache memoryCache;
    MockSettings settings;

    int wireLength = 1000;
    int freshness = 1000;
    settings.keyChain_ = &keyChain;
    settings.memoryCache_ = &memoryCache;
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d"), packetName(filter);
    packetName.appendSequenceNumber(0);

    OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback,
                                    &memoryCache, _1, _2, _3, _4, _5));
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingForName =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (p->getInterest()->matchesName(name))
                    interests.push_back(p);
        };
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingWithPrefix =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (name.match(p->getInterest()->getName()))
                    interests.push_back(p);
        };

    std::vector<Data> dataObjects;
    std::vector<Data> nacks;
    boost::function<void(const Data &)> mockAddData = [&dataObjects, &nacks, &pendingInterests, 
                                                       packetName, wireLength, freshness](const Data &data)
    {
        EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
        EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
        if (data.getMetaInfo().getType() == ndn_ContentType_BLOB)
            dataObjects.push_back(data);
        else if (data.getMetaInfo().getType() == ndn_ContentType_NACK)
            nacks.push_back(data);

        int i = 0;
        while (i < pendingInterests.size())
        {
            if (pendingInterests[i]->getInterest()->getName().match(data.getName()))
                pendingInterests.erase(pendingInterests.begin() + i);
            else
                i++;
        }

#if ADD_CRC
        // check CRC value
        NetworkData nd(*data.getContent());
        uint64_t crcValue = data.getName()[-1].toNumber();
        EXPECT_EQ(crcValue, nd.getCrcValue());
#endif
    };

    EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
    EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
        .Times(0);
    EXPECT_CALL(keyChain, sign(_))
        .Times(AtLeast(1));
    EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingForName));
    EXPECT_CALL(memoryCache, getPendingInterestsWithPrefix(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingWithPrefix));
    EXPECT_CALL(memoryCache, add(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockAddData));

    PacketPublisher<VideoFrameSegment, MockSettings> publisher(settings);
#ifdef ENABLE_LOGGING
    publisher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

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

        { // test one pending interest
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            dataObjects.clear();
            nacks.clear();

            for (int i = 0; i < 1; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            EXPECT_GE(ndn_getNowMilliseconds() - pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
            EXPECT_EQ(nonce, segHdr.interestNonce_);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(0, nacks.size());
        }
        { // test without passing a header
            dataObjects.clear();
            nacks.clear();

            PublishedDataPtrVector segments = publisher.publish(packetName, vp);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(0, nacks.size());
        }
        { // test two pending interests
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            dataObjects.clear();
            nacks.clear();

            for (int i = 0; i < 2; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(0, nacks.size());
        }
        { // test no pending interests
            dataObjects.clear();
            nacks.clear();

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            pendingInterests.clear();

            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            EXPECT_EQ(0, segHdr.generationDelayMs_);
            EXPECT_EQ(0, segHdr.interestArrivalMs_);
            EXPECT_EQ(0, segHdr.interestNonce_);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(0, nacks.size());
        }
        { // test many pending interests
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            dataObjects.clear();
            nacks.clear();

            for (int i = 0; i < 5 + segHdr.totalSegmentsNum_; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(5, nacks.size());
        }
    }
}

TEST(TestPacketPublisher, TestPublishVideoParity)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d/%FE%00", 2000);
    boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
    PendingInterests pendingInterests;

    uint32_t nonce = 1234;
    interest->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));

    MockNdnKeyChain keyChain;
    MockNdnMemoryCache memoryCache;
    MockSettings settings;

    int wireLength = 1000;
    int freshness = 1000;
    settings.keyChain_ = &keyChain;
    settings.memoryCache_ = &memoryCache;
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d");
    Name packetName(filter);
    packetName.appendSequenceNumber(0);

    OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingForName =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (p->getInterest()->matchesName(name))
                    interests.push_back(p);
        };
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingWithPrefix =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (name.match(p->getInterest()->getName()))
                    interests.push_back(p);
        };

    std::vector<Data> dataObjects;
    std::vector<Data> nacks;
    boost::function<void(const Data &)> mockAddData = [&dataObjects, &nacks, &pendingInterests, packetName, wireLength, freshness](const Data &data) {
        EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
        EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
        if (data.getMetaInfo().getType() == ndn_ContentType_BLOB)
            dataObjects.push_back(data);
        else if (data.getMetaInfo().getType() == ndn_ContentType_NACK)
            nacks.push_back(data);

        int i = 0;
        while (i < pendingInterests.size())
        {
            if (pendingInterests[i]->getInterest()->getName().match(data.getName()))
                pendingInterests.erase(pendingInterests.begin() + i);
            else
                i++;
        }

#if ADD_CRC
        // check CRC value
        NetworkData nd(*data.getContent());
        uint64_t crcValue = data.getName()[-1].toNumber();
        EXPECT_EQ(crcValue, nd.getCrcValue());
#endif
    };

    EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
    EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
        .Times(0); // no filter provided
    EXPECT_CALL(keyChain, sign(_))
        .Times(AtLeast(1));
    EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingForName));
    EXPECT_CALL(memoryCache, getPendingInterestsWithPrefix(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingWithPrefix));
    EXPECT_CALL(memoryCache, add(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockAddData));

    PacketPublisher<CommonSegment, MockSettings> publisher(settings);
#ifdef ENABLE_LOGGING
    publisher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

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

        { // test one pending interest
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            DataSegmentHeader segHdr;

            for (int i = 0; i < 1; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            PublishedDataPtrVector segments = publisher.publish(packetName, *parityData, segHdr, freshness);
            EXPECT_GE(ndn_getNowMilliseconds() - pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
            EXPECT_EQ(nonce, segHdr.interestNonce_);
            EXPECT_EQ(segments.size(), dataObjects.size());
        }
        { // test without segment header
            dataObjects.clear();
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

            PublishedDataPtrVector segments = publisher.publish(packetName, *parityData);
            EXPECT_EQ(segments.size(), dataObjects.size());
        }
    }
}

TEST(TestPacketPublisher, TestPublishAudioBundle)
{
    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d/%FE%00", 2000);
    boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
    PendingInterests pendingInterests;

    uint32_t nonce = 1234;
    interest->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
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
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d"), packetName(filter);
    packetName.appendSequenceNumber(0);

    OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingForName =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (p->getInterest()->matchesName(name))
                    interests.push_back(p);
        };
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingWithPrefix =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (name.match(p->getInterest()->getName()))
                    interests.push_back(p);
        };

    std::vector<Data> dataObjects;
    std::vector<Data> nacks;
    boost::function<void(const Data &)> mockAddData = [&dataObjects, &nacks, &pendingInterests, packetName, wireLength, freshness](const Data &data) {
        EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
        EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
        if (data.getMetaInfo().getType() == ndn_ContentType_BLOB)
            dataObjects.push_back(data);
        else if (data.getMetaInfo().getType() == ndn_ContentType_NACK)
            nacks.push_back(data);

        int i = 0;
        while (i < pendingInterests.size())
        {
            if (pendingInterests[i]->getInterest()->getName().match(data.getName()))
                pendingInterests.erase(pendingInterests.begin() + i);
            else
                i++;
        }

#if ADD_CRC
        // check CRC value
        NetworkData nd(*data.getContent());
        uint64_t crcValue = data.getName()[-1].toNumber();
        EXPECT_EQ(crcValue, nd.getCrcValue());
#endif
    };

    EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
    EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
        .Times(0);
    EXPECT_CALL(keyChain, sign(_))
        .Times(AtLeast(1));
    EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingForName));
    EXPECT_CALL(memoryCache, getPendingInterestsWithPrefix(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingWithPrefix));
    EXPECT_CALL(memoryCache, add(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockAddData));

    PacketPublisher<CommonSegment, MockSettings> publisher(settings);

    {
        std::vector<uint8_t> rtpData;
        for (int i = 0; i < data_len; ++i)
            rtpData.push_back((uint8_t)i);

        int wire_len = wireLength;
        AudioBundlePacket bundlePacket(wireLength);
        AudioBundlePacket::AudioSampleBlob sample({false}, rtpData.begin(), rtpData.end());

        while (bundlePacket.hasSpace(sample))
            bundlePacket << sample;

        CommonHeader hdr;
        hdr.sampleRate_ = 24.7;
        hdr.publishTimestampMs_ = 488589553;
        hdr.publishUnixTimestamp_ = 1460488589;

        bundlePacket.setHeader(hdr);

        for (int i = 0; i < 1; ++i)
        {
            boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
            in->getName().appendSegment(i);
            in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
            boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
            pendingInterests.push_back(pi);
        }

        DataSegmentHeader segHdr;
        PublishedDataPtrVector segments = publisher.publish(packetName, bundlePacket, segHdr, freshness);
        EXPECT_GE(ndn_getNowMilliseconds() - pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
        EXPECT_EQ(nonce, segHdr.interestNonce_);
        EXPECT_EQ(segments.size(), dataObjects.size());
        GT_PRINTF("Targeting %d wire length segment, audio bundle wire length is %d bytes "
                  "(for sample size %d, bundle fits %d samples total)\n",
                  wire_len, bundlePacket.getLength(),
                  data_len, bundlePacket.getSamplesNum());
    }
}

TEST(TestPacketPublisher, TestPitDeepClean)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d/%FE%01", 2000);
    boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
    PendingInterests pendingInterests;

    uint32_t nonce = 1234;
    interest->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));

    MockNdnKeyChain keyChain;
    MockNdnMemoryCache memoryCache;
    MockSettings settings;

    int wireLength = 1000;
    int freshness = 1000;
    settings.keyChain_ = &keyChain;
    settings.memoryCache_ = &memoryCache;
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d"), packetName(filter);
    packetName.appendSequenceNumber(1);

    OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingForName =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (p->getInterest()->matchesName(name))
                    interests.push_back(p);
        };
    boost::function<void(const Name &, PendingInterests &)> mockGetPendingWithPrefix =
        [&pendingInterests](const Name &name, PendingInterests &interests) {
            interests.clear();
            for (auto p : pendingInterests)
                if (name.match(p->getInterest()->getName()))
                    interests.push_back(p);
        };

    std::vector<Data> dataObjects;
    std::vector<Data> nacks;
    boost::function<void(const Data &)> mockAddData = [&dataObjects, &nacks, &pendingInterests, packetName, wireLength, freshness](const Data &data) {
        EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
        if (data.getMetaInfo().getType() == ndn_ContentType_BLOB)
            dataObjects.push_back(data);
        else if (data.getMetaInfo().getType() == ndn_ContentType_NACK)
            nacks.push_back(data);

        int i = 0;
        while (i < pendingInterests.size())
        {
            if (pendingInterests[i]->getInterest()->getName().match(data.getName()))
                pendingInterests.erase(pendingInterests.begin() + i);
            else
                i++;
        }

#if ADD_CRC
        // check CRC value
        NetworkData nd(*data.getContent());
        uint64_t crcValue = data.getName()[-1].toNumber();
        EXPECT_EQ(crcValue, nd.getCrcValue());
#endif
    };

    EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
    EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
        .Times(0);
    EXPECT_CALL(keyChain, sign(_))
        .Times(AtLeast(1));
    EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingForName));
    EXPECT_CALL(memoryCache, getPendingInterestsWithPrefix(_, _))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockGetPendingWithPrefix));
    EXPECT_CALL(memoryCache, add(_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(mockAddData));

    PacketPublisher<VideoFrameSegment, MockSettings> publisher(settings);
#ifdef ENABLE_LOGGING
    publisher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

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

        {
            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            dataObjects.clear();
            nacks.clear();

            // add pending interests for previous sample
            for (int i = 0; i < 10; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(Name("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/tiny/d/%FE%00"));
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            // now add few interests for published sample
            for (int i = 0; i < 5 + segHdr.totalSegmentsNum_; ++i)
            {
                boost::shared_ptr<Interest> in = boost::make_shared<Interest>(interest->getName());
                in->getName().appendSegment(i);
                in->setNonce(Blob((uint8_t *)&nonce, sizeof(nonce)));
                boost::shared_ptr<MemoryContentCache::PendingInterest> pi = boost::make_shared<MemoryContentCache::PendingInterest>(in, face);
                pendingInterests.push_back(pi);
            }

            // passing last argument as true should force deep PIT cleaning
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness, true);
            EXPECT_EQ(segments.size(), dataObjects.size());
            EXPECT_EQ(15, nacks.size()); // as a result, we shall receive 10 (old sample) + 5 (current sample) NACKs
        }
    }
}

TEST(TestPacketPublisher, TestBenchmarkSigningSegment1000)
{
    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    PublisherSettings settings;

    int wireLength = 1000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/test"), packetName(filter);
    packetName.append("1");

    for (int frameLen = 10000; frameLen < 35000; frameLen += 10000)
    {
        VideoPacketPublisher publisher(settings);
        int nFrames = 100;
        unsigned int publishDuration = 0;
        unsigned int totalSlices = 0;

        for (int i = 0; i < nFrames; ++i)
        {
            CommonHeader hdr;
            hdr.sampleRate_ = 24.7;
            hdr.publishTimestampMs_ = 488589553;
            hdr.publishUnixTimestamp_ = 1460488589;

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

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2 - t1).count();
            totalSlices += segments.size();
        }

        GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.2fms\n",
                  nFrames, frameLen, (double)totalSlices / (double)nFrames, (double)publishDuration / (double)nFrames);
    }
}

TEST(TestPacketPublisher, TestBenchmarkSigningSegment8000)
{
    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    PublisherSettings settings;

    int wireLength = 8000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

    Name filter("/test"), packetName(filter);
    packetName.append("1");

    for (int frameLen = 10000; frameLen < 35000; frameLen += 10000)
    {
        VideoPacketPublisher publisher(settings);
        int nFrames = 100;
        unsigned int publishDuration = 0;
        unsigned int totalSlices = 0;

        for (int i = 0; i < nFrames; ++i)
        {
            CommonHeader hdr;
            hdr.sampleRate_ = 24.7;
            hdr.publishTimestampMs_ = 488589553;
            hdr.publishUnixTimestamp_ = 1460488589;

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

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2 - t1).count();
            totalSlices += segments.size();
        }

        GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.2fms\n",
                  nFrames, frameLen, (double)totalSlices / (double)nFrames, (double)publishDuration / (double)nFrames);
    }
}

TEST(TestPacketPublisher, TestBenchmarkNoSigning)
{
    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    PublisherSettings settings;

    int wireLength = 1000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.sign_ = false;

    Name filter("/test"), packetName(filter);
    packetName.append("1");

    for (int frameLen = 10000; frameLen < 35000; frameLen += 10000)
    {
        VideoPacketPublisher publisher(settings);
        int nFrames = 1000;
        unsigned int publishDuration = 0;
        unsigned int totalSlices = 0;

        for (int i = 0; i < nFrames; ++i)
        {
            CommonHeader hdr;
            hdr.sampleRate_ = 24.7;
            hdr.publishTimestampMs_ = 488589553;
            hdr.publishUnixTimestamp_ = 1460488589;

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

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2 - t1).count();
            totalSlices += segments.size();
        }

        GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.6fms\n",
                  nFrames, frameLen, (double)totalSlices / (double)nFrames, (double)publishDuration / (double)nFrames);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
