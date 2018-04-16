//
// test-pipeline-control-state-machine.h
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "pipeliner.hpp"
#include "latency-control.hpp"
#include "interest-control.hpp"
#include "pipeline-control-state-machine.hpp"
#include "sample-estimator.hpp"

#include "tests-helpers.hpp"

#include "mock-objects/interest-control-mock.hpp"
#include "mock-objects/pipeliner-mock.hpp"
#include "mock-objects/latency-control-mock.hpp"
#include "mock-objects/buffer-mock.hpp"
#include "mock-objects/playout-control-mock.hpp"
#include "mock-objects/pipeline-control-state-machine-observer-mock.hpp"

#define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

// TODO: comment out until have better idea about unit-testing this
#if 0
TEST(TestPipelineControlStateMachine, TestDefaultSequenceVideo)
{
    /** 
     * The scenario tested here is as follows:
     * - state machine for video thread consumer is created
     * - this initializes/resets structures:
     *   - buffer
     *   - pipeliner 
     *   - interest control
     *   - latency control
     *   - playout control
     * - state machine receives Start event, this:
     *   - asks pipeliner for sending interest for metadata
     *   - enters Bootstrapping state
     * - when (fake) metadata received, it:
     *   - initializes sequence counters for delta and key frames in pipeliner
     *   - initializes interest control pipeline limit
     *   - asks pipeliner to issue interest for delta and key frames
     *   - enters Adjusting state
     * - when frame segment arrives:
     *   - pipeline size adjusting processs starts:
     *     - state machine questions latency control whether pipeline needs to be changes
     *     - state machine stays in adjusting mode until PipelineIncrease event received from latency control
     *     - in that case, state machine enters Fetching mode
     */
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockInterestControlStrategy> strategy(boost::make_shared<MockInterestControlStrategy>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(storage));

    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
    ctrl.sampleEstimator_ = sampleEstimator;

    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, getCurrentStrategy())
        .WillRepeatedly(Return(boost::shared_ptr<IInterestControlStrategy>(new InterestControl::StrategyDefault())));
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    MockPipelineControlStateMachineObserver observer;
    PipelineControlStateMachine sm = PipelineControlStateMachine::videoStateMachine(ctrl);
    EXPECT_EQ(kStateIdle, sm.getState());
    sm.attach(&observer);

#ifdef ENABLE_LOGGING
    sm.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    EXPECT_CALL(*pp, setNeedMetadata())
        .Times(1);
    EXPECT_CALL(*pp, express(Name(threadPrefix), false))
        .Times(1);
    EXPECT_CALL(observer, onStateMachineChangedState(_, "Bootstrapping"))
        .Times(1)
        .WillOnce(Invoke([](const boost::shared_ptr<const ndnrtc::PipelineControlEvent> &s, std::string newState) {
            EXPECT_EQ(s->getType(), PipelineControlEvent::Start);
        }));

    sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
    EXPECT_EQ(kStateBootstrapping, sm.getState());

    int startSeqNoDelta = 234;
    int startSeqNoKey = 7;
    int startGopPos = 23;
    boost::shared_ptr<WireSegment> seg =
        getFakeThreadMetadataSegment(threadPrefix,
                                     VideoThreadMeta(30, startSeqNoDelta, startSeqNoKey, startGopPos,
                                                     FrameSegmentsInfo(7, 3, 35, 5),
                                                     sampleVideoCoderParams()));
    int pipelineInitial = 0;

    {
        InSequence seq;
        EXPECT_CALL(*interestControl, getCurrentStrategy())
            .WillOnce(Return(strategy));
        EXPECT_CALL(*strategy, calculateDemand(_, _, _))
            .WillOnce(Return(3));
        EXPECT_CALL(*interestControl, initialize(30, _))
            .WillOnce(Invoke([&pipelineInitial, startGopPos](double rate, int pi) {
                pipelineInitial = pi;
                GT_PRINTF("Pipeline initial size %d\n", pi);
                EXPECT_GT(pipelineInitial, 30 - startGopPos);
            }));
        EXPECT_CALL(*pp, setSequenceNumber(startSeqNoDelta, SampleClass::Delta));
        EXPECT_CALL(*pp, setSequenceNumber(startSeqNoKey + 1, SampleClass::Key));
        EXPECT_CALL(*pp, setNeedSample(SampleClass::Key));
        EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)));
    }

    sm.dispatch(boost::make_shared<EventSegment>(seg));
    EXPECT_EQ(kStateBootstrapping, sm.getState());

    boost::shared_ptr<WireSegment> dataSeg = getFakeSegment(threadPrefix, SampleClass::Delta, SegmentClass::Data,
                                                            startSeqNoDelta, 0);

    EXPECT_CALL(*interestControl, pipelineLimit())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(true, _))
        .Times(1);
    EXPECT_CALL(*pp, segmentArrived(_))
        .Times(1);

    EXPECT_CALL(observer, onStateMachineChangedState(_, "Adjusting"));
    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));


    EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
        .Times(3);
    EXPECT_CALL(*latencyControl, getCurrentCommand())
        .Times(3)
        .WillOnce(Return(PipelineAdjust::KeepPipeline))
        .WillOnce(Return(PipelineAdjust::DecreasePipeline))
        .WillOnce(Return(PipelineAdjust::IncreasePipeline));
    EXPECT_CALL(*interestControl, pipelineLimit())
        .Times(1)
        .WillOnce(Return(3));
    EXPECT_CALL(*interestControl, markLowerLimit(3))
        .Times(1);

    boost::shared_ptr<WireSegment> dataSeg = getFakeSegment(threadPrefix, SampleClass::Delta, SegmentClass::Data,
                                                            startSeqNoDelta, 7);

    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateAdjusting, sm.getState());

    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateAdjusting, sm.getState());

    EXPECT_CALL(observer, onStateMachineChangedState(_, "Fetching"));
    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateFetching, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestDefaultSequenceAudio)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());

    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;

    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    MockPipelineControlStateMachineObserver observer;
    PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
    EXPECT_EQ(kStateIdle, sm.getState());
    sm.attach(&observer);

#ifdef ENABLE_LOGGING
    sm.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    EXPECT_CALL(*pp, setNeedMetadata())
        .Times(1);
    EXPECT_CALL(*pp, express(Name(threadPrefix), false))
        .Times(1);
    EXPECT_CALL(observer, onStateMachineChangedState(_, "Bootstrapping"))
        .Times(1)
        .WillOnce(Invoke([](const boost::shared_ptr<const ndnrtc::PipelineControlEvent> &s, std::string newState) {
            EXPECT_EQ(s->getType(), PipelineControlEvent::Start);
        }));

    sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
    EXPECT_EQ(kStateBootstrapping, sm.getState());

    int startSeqNo = 234;
    boost::shared_ptr<WireSegment> seg =
        getFakeThreadMetadataSegment(threadPrefix, AudioThreadMeta(30, startSeqNo, "opus"));

    int pipelineInitial = 0;

    {
        InSequence seq;
        EXPECT_CALL(*interestControl, initialize(30, _))
            .WillOnce(Invoke([&pipelineInitial](double rate, int pi) {
                pipelineInitial = pi;
                GT_PRINTF("Pipeline initial size %d\n", pi);
                EXPECT_GT(pipelineInitial, InterestControl::MinPipelineSize);
            }));
        EXPECT_CALL(*pp, setSequenceNumber(startSeqNo, SampleClass::Delta));
        EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
        EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)));
    }

    EXPECT_CALL(*interestControl, pipelineLimit())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(true, _))
        .Times(1);

    EXPECT_CALL(observer, onStateMachineChangedState(_, "Adjusting"));
    sm.dispatch(boost::make_shared<EventSegment>(seg));
    EXPECT_EQ(kStateAdjusting, sm.getState());

    EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
        .Times(3);
    EXPECT_CALL(*latencyControl, getCurrentCommand())
        .Times(3)
        .WillOnce(Return(PipelineAdjust::KeepPipeline))
        .WillOnce(Return(PipelineAdjust::DecreasePipeline))
        .WillOnce(Return(PipelineAdjust::IncreasePipeline));
    EXPECT_CALL(*interestControl, pipelineLimit())
        .Times(1)
        .WillOnce(Return(3));
    EXPECT_CALL(*interestControl, markLowerLimit(3))
        .Times(1);

    boost::shared_ptr<WireSegment> dataSeg = getFakeSegment(threadPrefix, SampleClass::Delta, SegmentClass::Data,
                                                            startSeqNo, 1);

    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateAdjusting, sm.getState());

    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateAdjusting, sm.getState());

    EXPECT_CALL(observer, onStateMachineChangedState(_, "Fetching"));
    sm.dispatch(boost::make_shared<EventSegment>(dataSeg));
    EXPECT_EQ(kStateFetching, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestBootstrapTimeout)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    Name prefix("/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/");
    prefix.appendVersion(NameComponents::nameApiVersion()).append(Name("video/camera/hi"));
    std::string threadPrefix = prefix.toUri();
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<StatisticsStorage> storage(StatisticsStorage::createConsumerStatistics());
    boost::shared_ptr<SampleEstimator> sampleEstimator(boost::make_shared<SampleEstimator>(storage));

    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
    ctrl.sampleEstimator_ = sampleEstimator;

    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*pp, reset())
        .Times(1);
    EXPECT_CALL(*interestControl, reset())
        .Times(1);
    EXPECT_CALL(*latencyControl, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false, _))
        .Times(1);

    MockPipelineControlStateMachineObserver observer;
    PipelineControlStateMachine sm = PipelineControlStateMachine::videoStateMachine(ctrl);
    EXPECT_EQ(kStateIdle, sm.getState());
    sm.attach(&observer);

#ifdef ENABLE_LOGGING
    sm.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    EXPECT_CALL(*pp, setNeedMetadata())
        .Times(1);
    EXPECT_CALL(*pp, express(Name(threadPrefix), false))
        .Times(1);
    EXPECT_CALL(observer, onStateMachineChangedState(_, "Bootstrapping"))
        .Times(1)
        .WillOnce(Invoke([](const boost::shared_ptr<const ndnrtc::PipelineControlEvent> &s, std::string newState) {
            EXPECT_EQ(s->getType(), PipelineControlEvent::Start);
        }));

    sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
    EXPECT_EQ(kStateBootstrapping, sm.getState());

    // when timeout is received, we expect machine to re-enter bootstrapping phase,
    // thus causing new request for metadata
    EXPECT_CALL(*pp, setNeedMetadata())
        .Times(1);
    EXPECT_CALL(*pp, express(Name(threadPrefix), false))
        .Times(1);
    EXPECT_CALL(observer, onStateMachineReceivedEvent(_, _))
        .Times(1);

    Name fName(threadPrefix);
    fName.append(NameComponents::NameComponentDelta).appendSequenceNumber(7).appendSegment(0);
    NamespaceInfo ninfo;
    ASSERT_TRUE(NameComponents::extractInfo(fName, ninfo));

    sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
    EXPECT_EQ(kStateBootstrapping, sm.getState());
}
#endif
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
