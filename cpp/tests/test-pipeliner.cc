//
// test-pipeliner.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <ndn-cpp/data.hpp>

#include "gtest/gtest.h"
#include "pipeliner.hpp"
#include "sample-estimator.hpp"
#include "frame-data.hpp"
#include "statistics.hpp"

#include "tests-helpers.hpp"

#include "mock-objects/interest-control-mock.hpp"
#include "mock-objects/interest-queue-mock.hpp"
#include "mock-objects/playback-queue-mock.hpp"
#include "mock-objects/segment-controller-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

// #define ENABLE_LOGGING

TEST(TestPipeliner, TestRequestMetadata)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<statistics::StatisticsStorage> sstorage(statistics::StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(sstorage));
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage));
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockInterestQueue> interestQueue(boost::make_shared<MockInterestQueue>());
    boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());
    boost::shared_ptr<MockSegmentController> segmentController(boost::make_shared<MockSegmentController>());
    PipelinerSettings ppSettings({1000, sampleEstimator, buffer, interestControl,
                                  interestQueue, playbackQueue, segmentController, sstorage});

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));

    {
        Pipeliner pp(ppSettings, boost::make_shared<Pipeliner::AudioNameScheme>());

#ifdef ENABLE_LOGGING
        pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

        OnData onData = [](const boost::shared_ptr<const ndn::Interest> &,
                           const boost::shared_ptr<ndn::Data> &) {};
        OnTimeout onTimeout = [](const boost::shared_ptr<const ndn::Interest> &i) {};

        EXPECT_CALL(*segmentController, getOnDataCallback())
            .Times(2)
            .WillRepeatedly(Return(onData));
        EXPECT_CALL(*segmentController, getOnTimeoutCallback())
            .Times(2)
            .WillRepeatedly(Return(onTimeout));
        EXPECT_CALL(*segmentController, getOnNetworkNackCallback())
            .Times(2);

        EXPECT_CALL(*interestQueue, enqueueInterest(_, _, _, _, _))
            .Times(2)
            .WillRepeatedly(Invoke([prefix](const boost::shared_ptr<const ndn::Interest> &i,
                                            boost::shared_ptr<ndnrtc::DeadlinePriority>, OnData, OnTimeout, OnNetworkNack) {
                Name n(prefix);
                n.append(NameComponents::NameComponentMeta).appendVersion(0).appendSegment(0);
                EXPECT_EQ(n, i->getName());
                EXPECT_TRUE(i->getMustBeFresh());
            }));

        pp.setNeedMetadata();
        pp.express(prefix);
        pp.setNeedMetadata();
        pp.express(prefix);
    }

    {
        Pipeliner pp(ppSettings, boost::make_shared<Pipeliner::VideoNameScheme>());

#ifdef ENABLE_LOGGING
        pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

        OnData onData = [](const boost::shared_ptr<const ndn::Interest> &,
                           const boost::shared_ptr<ndn::Data> &) {};
        OnTimeout onTimeout = [](const boost::shared_ptr<const ndn::Interest> &i) {};

        EXPECT_CALL(*segmentController, getOnDataCallback())
            .Times(2)
            .WillRepeatedly(Return(onData));
        EXPECT_CALL(*segmentController, getOnTimeoutCallback())
            .Times(2)
            .WillRepeatedly(Return(onTimeout));
        EXPECT_CALL(*segmentController, getOnNetworkNackCallback())
            .Times(2);

        EXPECT_CALL(*interestQueue, enqueueInterest(_, _, _, _, _))
            .Times(2)
            .WillRepeatedly(Invoke([prefix](const boost::shared_ptr<const ndn::Interest> &i,
                                            boost::shared_ptr<ndnrtc::DeadlinePriority>, OnData, OnTimeout, OnNetworkNack) {
                Name n(prefix);
                n.append(NameComponents::NameComponentMeta).appendVersion(0).appendSegment(0);
                EXPECT_EQ(n, i->getName());
                EXPECT_TRUE(i->getMustBeFresh());
            }));

        pp.setNeedMetadata();
        pp.express(prefix);
        pp.setNeedMetadata();
        pp.express(prefix);
    }
}

TEST(TestPipeliner, TestRequestSample)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<statistics::StatisticsStorage> sstorage(statistics::StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(sstorage));
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage));
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockInterestQueue> interestQueue(boost::make_shared<MockInterestQueue>());
    boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());
    boost::shared_ptr<MockSegmentController> segmentController(boost::make_shared<MockSegmentController>());
    PipelinerSettings ppSettings({1000, sampleEstimator, buffer, interestControl,
                                  interestQueue, playbackQueue, segmentController, sstorage});

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));

    Pipeliner pp(ppSettings, boost::make_shared<Pipeliner::VideoNameScheme>());

#ifdef ENABLE_LOGGING
    pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    OnData onData = [](const boost::shared_ptr<const ndn::Interest> &,
                       const boost::shared_ptr<ndn::Data> &) {};
    OnTimeout onTimeout = [](const boost::shared_ptr<const ndn::Interest> &i) {};

    boost::shared_ptr<WireSegment>
        dataSegment = getFakeSegment(prefix.toUri(), SampleClass::Delta, SegmentClass::Data, 7, 0),
        paritySegment = getFakeSegment(prefix.toUri(), SampleClass::Delta, SegmentClass::Parity, 7, 0);

    sampleEstimator->segmentArrived(dataSegment);
    sampleEstimator->segmentArrived(paritySegment);

    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Data), 10);
    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Parity), 2);

    for (int i = 0; i < 2; ++i)
    {
        EXPECT_CALL(*segmentController, getOnDataCallback())
            .Times(12)
            .WillRepeatedly(Return(onData));
        EXPECT_CALL(*segmentController, getOnTimeoutCallback())
            .Times(12)
            .WillRepeatedly(Return(onTimeout));
        EXPECT_CALL(*segmentController, getOnNetworkNackCallback())
            .Times(12);

        int segNo = 0;
        EXPECT_CALL(*interestQueue, enqueueInterest(_, _, _, _, _))
            .Times(12)
            .WillRepeatedly(Invoke([prefix, &segNo](const boost::shared_ptr<const ndn::Interest> &i,
                                                    boost::shared_ptr<ndnrtc::DeadlinePriority>, OnData, OnTimeout, OnNetworkNack) {
                Name n(prefix);
                if (segNo < 10)
                    EXPECT_EQ(n.append(NameComponents::NameComponentDelta).appendSequenceNumber(7).appendSegment(segNo), i->getName());
                else
                    EXPECT_EQ(n.append(NameComponents::NameComponentDelta).appendSequenceNumber(7).append(NameComponents::NameComponentParity).appendSegment(segNo - 10), i->getName());
                segNo++;
            }));

        pp.setSequenceNumber(7, SampleClass::Delta);
        pp.express(prefix);
    }
}

TEST(TestPipeliner, TestRequestKeySample)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<statistics::StatisticsStorage> sstorage(statistics::StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(sstorage));
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage));
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockInterestQueue> interestQueue(boost::make_shared<MockInterestQueue>());
    boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());
    boost::shared_ptr<MockSegmentController> segmentController(boost::make_shared<MockSegmentController>());
    PipelinerSettings ppSettings({1000, sampleEstimator, buffer, interestControl,
                                  interestQueue, playbackQueue, segmentController, sstorage});

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));

    Pipeliner pp(ppSettings, boost::make_shared<Pipeliner::VideoNameScheme>());

#ifdef ENABLE_LOGGING
    pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    OnData onData = [](const boost::shared_ptr<const ndn::Interest> &,
                       const boost::shared_ptr<ndn::Data> &) {};
    OnTimeout onTimeout = [](const boost::shared_ptr<const ndn::Interest> &i) {};

    boost::shared_ptr<WireSegment>
        dataSegment = getFakeSegment(prefix.toUri(), SampleClass::Key, SegmentClass::Data, 7, 0),
        paritySegment = getFakeSegment(prefix.toUri(), SampleClass::Key, SegmentClass::Parity, 7, 0);

    sampleEstimator->segmentArrived(dataSegment);
    sampleEstimator->segmentArrived(paritySegment);

    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Data), 30);
    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Parity), 6);

    for (int i = 0; i < 2; ++i)
    {
        EXPECT_CALL(*segmentController, getOnDataCallback())
            .Times(36)
            .WillRepeatedly(Return(onData));
        EXPECT_CALL(*segmentController, getOnTimeoutCallback())
            .Times(36)
            .WillRepeatedly(Return(onTimeout));
        EXPECT_CALL(*segmentController, getOnNetworkNackCallback())
            .Times(36);

        int segNo = 0;
        EXPECT_CALL(*interestQueue, enqueueInterest(_, _, _, _, _))
            .Times(36)
            .WillRepeatedly(Invoke([prefix, &segNo](const boost::shared_ptr<const ndn::Interest> &i,
                                                    boost::shared_ptr<ndnrtc::DeadlinePriority>, OnData, OnTimeout, OnNetworkNack) {
                Name n(prefix);
                if (segNo < 30)
                    EXPECT_EQ(n.append(NameComponents::NameComponentKey).appendSequenceNumber(7).appendSegment(segNo), i->getName());
                else
                    EXPECT_EQ(n.append(NameComponents::NameComponentKey).appendSequenceNumber(7).append(NameComponents::NameComponentParity).appendSegment(segNo - 30), i->getName());
                segNo++;
            }));

        pp.setSequenceNumber(7, SampleClass::Key);
        pp.setNeedSample(SampleClass::Key);
        pp.express(prefix);
    }
}

TEST(TestPipeliner, TestSegmentsArrive)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<statistics::StatisticsStorage> sstorage(statistics::StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(sstorage));
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<Buffer> buffer(boost::make_shared<Buffer>(storage));
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockInterestQueue> interestQueue(boost::make_shared<MockInterestQueue>());
    boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());
    boost::shared_ptr<MockSegmentController> segmentController(boost::make_shared<MockSegmentController>());
    PipelinerSettings ppSettings({1000, sampleEstimator, buffer, interestControl,
                                  interestQueue, playbackQueue, segmentController, sstorage});

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));

    Pipeliner pp(ppSettings, boost::make_shared<Pipeliner::VideoNameScheme>());

#ifdef ENABLE_LOGGING
    pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    OnData onData = [](const boost::shared_ptr<const ndn::Interest> &,
                       const boost::shared_ptr<ndn::Data> &) {};
    OnTimeout onTimeout = [](const boost::shared_ptr<const ndn::Interest> &i) {};

    sampleEstimator->segmentArrived(getFakeSegment(prefix.toUri(), SampleClass::Delta, SegmentClass::Data, 7, 0));
    sampleEstimator->segmentArrived(getFakeSegment(prefix.toUri(), SampleClass::Delta, SegmentClass::Parity, 7, 0));
    sampleEstimator->segmentArrived(getFakeSegment(prefix.toUri(), SampleClass::Key, SegmentClass::Data, 7, 0));
    sampleEstimator->segmentArrived(getFakeSegment(prefix.toUri(), SampleClass::Key, SegmentClass::Parity, 7, 0));

    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Data), 30);
    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Key, SegmentClass::Parity), 6);
    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Data), 10);
    ASSERT_EQ(sampleEstimator->getSegmentNumberEstimation(SampleClass::Delta, SegmentClass::Parity), 2);

    pp.setSequenceNumber(7, SampleClass::Delta);
    pp.setSequenceNumber(1, SampleClass::Key);

    EXPECT_CALL(*playbackQueue, size())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(150));
    EXPECT_CALL(*playbackQueue, pendingSize())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(50));

    int roomSize = 5;

    EXPECT_CALL(*interestControl, snapshot())
        .Times(AtLeast(1));

    for (int i = 1; i <= 30; ++i)
    {
        int nExpectedDataInterests = (i == 30 ? 30 : 10);
        int nExpectedParityInterests = (i == 30 ? 6 : 2);

        EXPECT_CALL(*interestControl, room())
            .Times(AtLeast(roomSize + 1))
            .WillRepeatedly(Invoke([&roomSize]() -> size_t { return roomSize; }));

        EXPECT_CALL(*interestControl, increment())
            .Times(roomSize)
            .WillRepeatedly(Invoke([&roomSize, &i]() -> bool {
                --roomSize;
                return (roomSize > 0);
            }));
        EXPECT_CALL(*segmentController, getOnDataCallback())
            .Times((nExpectedDataInterests + nExpectedParityInterests) * roomSize)
            .WillRepeatedly(Return(onData));
        EXPECT_CALL(*segmentController, getOnTimeoutCallback())
            .Times((nExpectedDataInterests + nExpectedParityInterests) * roomSize)
            .WillRepeatedly(Return(onTimeout));
        EXPECT_CALL(*segmentController, getOnNetworkNackCallback())
            .Times((nExpectedDataInterests + nExpectedParityInterests) * roomSize);
        EXPECT_CALL(*interestQueue, enqueueInterest(_, _, _, _, _))
            .Times((nExpectedDataInterests + nExpectedParityInterests) * roomSize);

        if (i == 30)
            pp.setNeedSample(SampleClass::Key);
        pp.fillUpPipeline(prefix);
        roomSize++;
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
