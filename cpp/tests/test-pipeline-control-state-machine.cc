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

#define ENABLE_LOGGING

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
	boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
	boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
	boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
	
	PipelineControlStateMachine::Struct ctrl((Name(threadPrefix)));
	ctrl.pipeliner_ = pp;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;

	EXPECT_CALL(*pp, reset())
		.Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);

	PipelineControlStateMachine sm(ctrl);
	EXPECT_EQ(kStateIdle, sm.getState());

#ifdef ENABLE_LOGGING
	sm.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	EXPECT_CALL(*pp, setNeedRightmost())
		.Times(1);
	EXPECT_CALL(*pp, express(Name(threadPrefix)))
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
		EXPECT_CALL(*pp, express(Name(threadPrefix)));
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

// test timeout in rightmost
// test timeouts in wait initial
// test timeouts in adjusting

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
