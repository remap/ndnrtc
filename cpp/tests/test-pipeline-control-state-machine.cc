// 
// test-pipeline-control-state-machine.h
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "pipeliner.h"
#include "latency-control.h"
#include "interest-control.h"
#include "pipeline-control-state-machine.h"

#include "tests-helpers.h"

#include "mock-objects/interest-control-mock.h"
#include "mock-objects/pipeliner-mock.h"
#include "mock-objects/latency-control-mock.h"
#include "mock-objects/buffer-mock.h"
#include "mock-objects/playout-control-mock.h"

// #define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
using namespace ndn;

TEST(TestPipelineControlStateMachine, TestDefaultSequence)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
	boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
	boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
	boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
	
	PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
	ctrl.buffer_ = buffer;
	ctrl.pipeliner_ = pp;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

    EXPECT_CALL(*buffer, reset())
        .Times(1);
	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7;
	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta, 
		SegmentClass::Data, startSeqNo, 0);

	{
		InSequence seq;
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Delta));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
    
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());


	EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
		.Times(4);
	EXPECT_CALL(*latencyControl, getCurrentCommand())
		.Times(3)
		.WillOnce(Return(PipelineAdjust::KeepPipeline))
		.WillOnce(Return(PipelineAdjust::IncreasePipeline))
		.WillOnce(Return(PipelineAdjust::DecreasePipeline));
	EXPECT_CALL(*interestControl, pipelineLimit())
		.Times(1)
		.WillOnce(Return(5));

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());
	
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());

    EXPECT_CALL(*playoutControl, allowPlayout(true))
        .Times(1);
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

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateAdjusting, sm.getState());
	
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateAdjusting, sm.getState());

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateFetching, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestDefaultSequenceVideoConsumer)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::videoStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	// difference from default consumer - should ask for Key rightmost
	EXPECT_CALL(*pp, setNeedSample(SampleClass::Key))
		.Times(1);
	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7, deltaSeqNo = 47;
	ndnrtc::VideoFramePacket p = getVideoFramePacket(800);
	std::vector<ndnrtc::VideoFrameSegment> segments = sliceFrame(p,45,deltaSeqNo);
	Name fname(threadPrefix);
	fname.append(NameComponents::NameComponentKey).appendSequenceNumber(startSeqNo);
	std::vector<boost::shared_ptr<ndn::Data>> data = dataFromSegments(fname.toUri(), segments);
	ASSERT_EQ(1, data.size());
	std::vector<boost::shared_ptr<ndn::Interest>> interests = getInterests(fname.toUri(), 0, 1);

	boost::shared_ptr<WireData<VideoFrameSegmentHeader>> seg = 
		boost::make_shared<WireData<VideoFrameSegmentHeader>>(data.front(), interests.front());
	ASSERT_TRUE(seg.get());

	{
		InSequence seq;
		// difference from default consumer - should ask for Key initial
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Key));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Key));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());

	// difference from default consumer - sets sequence number for Delta upon receiving initial segment
	EXPECT_CALL(*pp, setSequenceNumber(deltaSeqNo, SampleClass::Delta));
    
	EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
		.Times(4);
	EXPECT_CALL(*latencyControl, getCurrentCommand())
		.Times(3)
		.WillOnce(Return(PipelineAdjust::KeepPipeline))
		.WillOnce(Return(PipelineAdjust::IncreasePipeline))
		.WillOnce(Return(PipelineAdjust::DecreasePipeline));
	EXPECT_CALL(*interestControl, pipelineLimit())
		.Times(1)
		.WillOnce(Return(5));
    EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1, SampleClass::Key));

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());
	
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateChasing, sm.getState());

    EXPECT_CALL(*playoutControl, allowPlayout(true))
        .Times(1);
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

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateAdjusting, sm.getState());
	
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateAdjusting, sm.getState());

	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateFetching, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestRightmostTimeout)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	EXPECT_CALL(*pp, setNeedRightmost());
	EXPECT_CALL(*pp, express(Name(threadPrefix), false));

	NamespaceInfo dummy;
	sm.dispatch(boost::make_shared<EventTimeout>(dummy));

	EXPECT_EQ(kStateWaitForRightmost, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestRightmostReset)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
	EXPECT_EQ(kStateIdle, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestRightmostStarvation)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Starvation));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestWaitInitialTimeout)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7;
	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta, 
		SegmentClass::Data, startSeqNo, 0);

	{
		InSequence seq;
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Delta));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());

	Name fName(threadPrefix);
	fName.append(NameComponents::NameComponentDelta).appendSequenceNumber(7).appendSegment(0);
	NamespaceInfo ninfo;
	ASSERT_TRUE(NameComponents::extractInfo(fName, ninfo));

	for (int i = 0; i < 3; ++i)
	{
		{
			InSequence seq;
			EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
			EXPECT_CALL(*pp, express(Name(threadPrefix), false));
		}
		sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
	}

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);
    

	sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
	EXPECT_EQ(kStateIdle, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestVideoConsumerWaitInitialTimeout)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::videoStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedSample(SampleClass::Key));
	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7;
	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Key, 
		SegmentClass::Data, startSeqNo, 0);

	{
		InSequence seq;
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Key));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Key));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());

	Name fName(threadPrefix);
	fName.append(NameComponents::NameComponentKey).appendSequenceNumber(7).appendSegment(0);
	NamespaceInfo ninfo;
	ASSERT_TRUE(NameComponents::extractInfo(fName, ninfo));

	for (int i = 0; i < 3; ++i)
	{
		{
			InSequence seq;
			EXPECT_CALL(*pp, setNeedSample(SampleClass::Key));
			EXPECT_CALL(*pp, express(Name(threadPrefix), false));
		}
		sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
	}

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
	EXPECT_EQ(kStateIdle, sm.getState());

}

TEST(TestPipelineControlStateMachine, TestWaitInitialStarvation)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7;
	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta, 
		SegmentClass::Data, startSeqNo, 0);

	{
		InSequence seq;
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Delta));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Starvation));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());
	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Starvation));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());
	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Starvation));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestWaitInitialReset)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix), false))
		.Times(1);

	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
	EXPECT_EQ(kStateWaitForRightmost, sm.getState());

	int startSeqNo = 7;
	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta, 
		SegmentClass::Data, startSeqNo, 0);

	{
		InSequence seq;
		EXPECT_CALL(*pp, setSequenceNumber(startSeqNo+1,SampleClass::Delta));
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta));
		EXPECT_CALL(*pp, express(Name(threadPrefix), true));
        EXPECT_CALL(*interestControl, increment());
	}
	sm.dispatch(boost::make_shared<EventSegment>(seg));
	EXPECT_EQ(kStateWaitForInitial, sm.getState());
	
	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);
    
	sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
	EXPECT_EQ(kStateIdle, sm.getState());
}

TEST(TestPipelineControlStateMachine, TestIgnoredEvents)
{
    
    std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());
    boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
    boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
    boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    
    PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pp;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    
    EXPECT_CALL(*buffer, reset())
    .Times(1);
    EXPECT_CALL(*pp, reset())
    .Times(1);
    EXPECT_CALL(*interestControl, reset())
    .Times(1);
    EXPECT_CALL(*latencyControl, reset())
    .Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
    .Times(1);
    
    PipelineControlStateMachine sm = PipelineControlStateMachine::defaultStateMachine(ctrl);
    EXPECT_EQ(kStateIdle, sm.getState());
    
    boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta,
                                                        SegmentClass::Data, 7, 0);
    
    sm.dispatch(boost::make_shared<EventSegment>(seg));
    EXPECT_EQ(kStateIdle, sm.getState());
    
    Name fName(threadPrefix);
    fName.append(NameComponents::NameComponentDelta).appendSequenceNumber(7).appendSegment(0);
    NamespaceInfo ninfo;
    ASSERT_TRUE(NameComponents::extractInfo(fName, ninfo));
    
    sm.dispatch(boost::make_shared<EventTimeout>(ninfo));
    EXPECT_EQ(kStateIdle, sm.getState());
    sm.dispatch(boost::make_shared<EventStarvation>(500));
    EXPECT_EQ(kStateIdle, sm.getState());
    sm.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
    EXPECT_EQ(kStateIdle, sm.getState());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
