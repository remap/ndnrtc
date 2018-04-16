//
// test-pipeline-control.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "pipeline-control.hpp"
#include "tests-helpers.hpp"
#include "statistics.hpp"
#include "sample-estimator.hpp"

#include "mock-objects/interest-control-mock.hpp"
#include "mock-objects/pipeliner-mock.hpp"
#include "mock-objects/latency-control-mock.hpp"
#include "mock-objects/buffer-mock.hpp"
#include "mock-objects/playout-control-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

// #define ENABLE_LOGGING
// TODO: update this unit test
#if 0
TEST(TestPipelineControl, TestDefault)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(storage));

    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    PipelineControl ppc = PipelineControl::videoPipelineControl(Name(threadPrefix),
                                                                buffer, pp, interestControl, latencyControl, 
                                                                playoutControl, sampleEstimator, storage);

#ifdef ENABLE_LOGGING
    ppc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    int startSeqNoDelta = 234;
    int startSeqNoKey = 7;
    int startGopPos = 23;
    boost::shared_ptr<VideoThreadMeta> meta =
        boost::make_shared<VideoThreadMeta>(30, startSeqNoDelta, startSeqNoKey, startGopPos,
                                            FrameSegmentsInfo(7, 3, 35, 5),
                                            sampleVideoCoderParams());

    { // idle -> Adjusting
        EXPECT_CALL(*interestControl, initialize(_, _))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Delta))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, setNeedSample(SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, segmentArrived(_))
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(true, _))
            .Times(1);
        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(1);

        ppc.start();
    }

    boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta,
                                                        SegmentClass::Data, 7, 0);
    { // adjusting -> fetching
        EXPECT_CALL(*latencyControl, getCurrentCommand())
            .Times(4)
            .WillOnce(Return(PipelineAdjust::KeepPipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::IncreasePipeline));
        ;
        EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
            .Times(4);
        EXPECT_CALL(*interestControl, markLowerLimit(5))
            .Times(1);
        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(2)
            .WillRepeatedly(Return(5));

        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
    }
}

TEST(TestPipelineControl, TestDefaultWithBootstrapping)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(storage));

    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    PipelineControl ppc = PipelineControl::videoPipelineControl(Name(threadPrefix),
                                                                buffer, pp, interestControl, latencyControl, 
                                                                playoutControl, sampleEstimator, storage);

#ifdef ENABLE_LOGGING
    ppc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    {
        EXPECT_CALL(*pp, setNeedMetadata())
            .Times(1);
        EXPECT_CALL(*pp, express(Name(threadPrefix), false))
            .Times(1);

        ppc.start();
        EXPECT_ANY_THROW(ppc.start());

        EXPECT_CALL(*buffer, reset())
            .Times(1);
        EXPECT_CALL(*pp, reset())
            .Times(1);
        EXPECT_CALL(*interestControl, reset())
            .Times(1);
        EXPECT_CALL(*latencyControl, reset())
            .Times(1);

        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(false, _))
            .Times(1);

        ppc.stop();
    }

    { // idle -> Bootstrapping
        EXPECT_CALL(*pp, setNeedMetadata())
            .Times(1);
        EXPECT_CALL(*pp, express(Name(threadPrefix), false))
            .Times(1);
        ppc.start();
    }

    int startSeqNoDelta = 234;
    int startSeqNoKey = 7;
    int startGopPos = 23;
    boost::shared_ptr<WireSegment> metaSeg =
        getFakeThreadMetadataSegment(threadPrefix,
                                     VideoThreadMeta(30, startSeqNoDelta, startSeqNoKey, startGopPos,
                                                     FrameSegmentsInfo(7, 3, 35, 5),
                                                     sampleVideoCoderParams()));

    { // Bootstrapping -> Adjusting
        EXPECT_CALL(*interestControl, initialize(_, _))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Delta))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, setNeedSample(SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, segmentArrived(_))
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(true, _))
            .Times(1);

        ppc.segmentArrived(metaSeg);
    }

    boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta,
                                                        SegmentClass::Data, 7, 0);
    { // adjusting -> fetching
        EXPECT_CALL(*latencyControl, getCurrentCommand())
            .Times(4)
            .WillOnce(Return(PipelineAdjust::KeepPipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::IncreasePipeline));
        ;
        EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
            .Times(4);
        EXPECT_CALL(*interestControl, markLowerLimit(5))
            .Times(1);
        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(2)
            .WillRepeatedly(Return(5));

        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
    }
}

TEST(TestPipelineControl, TestStarvation)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(storage));

    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    PipelineControl ppc = PipelineControl::videoPipelineControl(Name(threadPrefix),
                                                                buffer, pp, interestControl, latencyControl, 
                                                                playoutControl, sampleEstimator, storage);

#ifdef ENABLE_LOGGING
    ppc.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    {
        EXPECT_CALL(*pp, setNeedMetadata())
            .Times(1);
        EXPECT_CALL(*pp, express(Name(threadPrefix), false))
            .Times(1);

        ppc.start();
        EXPECT_ANY_THROW(ppc.start());

        EXPECT_CALL(*buffer, reset())
            .Times(1);
        EXPECT_CALL(*pp, reset())
            .Times(1);
        EXPECT_CALL(*interestControl, reset())
            .Times(1);
        EXPECT_CALL(*latencyControl, reset())
            .Times(1);

        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(false, _))
            .Times(1);

        ppc.stop();
    }

    { // idle -> Bootstrapping
        EXPECT_CALL(*pp, setNeedMetadata())
            .Times(1);
        EXPECT_CALL(*pp, express(Name(threadPrefix), false))
            .Times(1);
        ppc.start();
    }

    int startSeqNoDelta = 234;
    int startSeqNoKey = 7;
    int startGopPos = 23;
    boost::shared_ptr<WireSegment> metaSeg =
        getFakeThreadMetadataSegment(threadPrefix,
                                     VideoThreadMeta(30, startSeqNoDelta, startSeqNoKey, startGopPos,
                                                     FrameSegmentsInfo(7, 3, 35, 5),
                                                     sampleVideoCoderParams()));

    { // Bootstrapping -> Adjusting
        EXPECT_CALL(*interestControl, initialize(_, _))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Delta))
            .Times(1);
        EXPECT_CALL(*pp, setSequenceNumber(_, SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, setNeedSample(SampleClass::Key))
            .Times(1);
        EXPECT_CALL(*pp, segmentArrived(_))
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(true, _))
            .Times(1);

        ppc.segmentArrived(metaSeg);
    }

    boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta,
                                                        SegmentClass::Data, 7, 0);
    { // adjusting -> fetching
        EXPECT_CALL(*latencyControl, getCurrentCommand())
            .Times(4)
            .WillOnce(Return(PipelineAdjust::KeepPipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::DecreasePipeline))
            .WillOnce(Return(PipelineAdjust::IncreasePipeline));
        ;
        EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
            .Times(4);
        EXPECT_CALL(*interestControl, markLowerLimit(5))
            .Times(1);
        EXPECT_CALL(*interestControl, pipelineLimit())
            .Times(2)
            .WillRepeatedly(Return(5));

        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
        ppc.segmentArrived(seg);
    }

    { // fetchin -> bootstrap
        EXPECT_CALL(*pp, reset())
            .Times(1);
        EXPECT_CALL(*buffer, reset())
            .Times(1);
        EXPECT_CALL(*interestControl, reset())
            .Times(1);
        EXPECT_CALL(*latencyControl, reset())
            .Times(1);
        EXPECT_CALL(*playoutControl, allowPlayout(false, _))
            .Times(1);
        EXPECT_CALL(*pp, setNeedMetadata())
            .Times(1);
        EXPECT_CALL(*pp, express(Name(threadPrefix), false))
            .Times(1);

        ppc.segmentStarvation();
    }
}
#endif
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
