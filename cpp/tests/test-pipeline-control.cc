// 
// test-pipeline-control.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "pipeline-control.h"
#include "tests-helpers.h"

#include "mock-objects/interest-control-mock.h"
#include "mock-objects/pipeliner-mock.h"
#include "mock-objects/latency-control-mock.h"
#include "mock-objects/buffer-mock.h"
#include "mock-objects/playout-control-mock.h"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndn;

TEST(TestPipelineControl, TestDefault)
{
	std::string threadPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi";
	boost::shared_ptr<MockPipeliner> pp(boost::make_shared<MockPipeliner>());
	boost::shared_ptr<MockInterestControl> interestControl(boost::make_shared<MockInterestControl>());
	boost::shared_ptr<MockLatencyControl> latencyControl(boost::make_shared<MockLatencyControl>());
    boost::shared_ptr<MockPlayoutControl> playoutControl(boost::make_shared<MockPlayoutControl>());
    boost::shared_ptr<MockBuffer> buffer(boost::make_shared<MockBuffer>());

	EXPECT_CALL(*pp, reset())
		.Times(1);
    EXPECT_CALL(*buffer, reset())
        .Times(1);
	EXPECT_CALL(*interestControl, reset())
		.Times(1);
	EXPECT_CALL(*latencyControl, reset())
		.Times(1);
    EXPECT_CALL(*playoutControl, allowPlayout(false))
        .Times(1);

	PipelineControl ppc = PipelineControl::defaultPipelineControl(Name(threadPrefix),
		buffer, pp, interestControl, latencyControl, playoutControl);

	{
		EXPECT_CALL(*pp, setNeedRightmost())
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
        EXPECT_CALL(*playoutControl, allowPlayout(false))
            .Times(1);
	
		ppc.stop();
	}

	{ // idle -> waitforrightmost
		EXPECT_CALL(*pp, setNeedRightmost())
			.Times(1);
		EXPECT_CALL(*pp, express(Name(threadPrefix), false))
			.Times(1);
		ppc.start();
	}

	boost::shared_ptr<WireSegment> seg = getFakeSegment(threadPrefix, SampleClass::Delta, 
		SegmentClass::Data, 7, 0);
	{ // waitforrightmost -> waitforinitial
		EXPECT_CALL(*pp, setSequenceNumber(8, SampleClass::Delta))
			.Times(1);
		EXPECT_CALL(*pp, setNeedSample(SampleClass::Delta))
			.Times(1);
		EXPECT_CALL(*pp, express(Name(threadPrefix), true))
			.Times(1);
        EXPECT_CALL(*interestControl, increment())
            .Times(1);

		ppc.segmentArrived(seg);
	}

	{ // waitforinitial -> chasing -> adjusting
        EXPECT_CALL(*playoutControl, allowPlayout(true))
            .Times(1);
		EXPECT_CALL(*latencyControl, getCurrentCommand())
			.Times(2)
			.WillOnce(Return(PipelineAdjust::IncreasePipeline))
			.WillOnce(Return(PipelineAdjust::DecreasePipeline));
		EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
			.Times(3);
		EXPECT_CALL(*interestControl, pipelineLimit())
			.Times(1)
			.WillOnce(Return(5));

		ppc.segmentArrived(seg);
		ppc.segmentArrived(seg);
		ppc.segmentArrived(seg);
	}

	{ // adjusting -> fetching
		EXPECT_CALL(*latencyControl, getCurrentCommand())
			.Times(1)
			.WillOnce(Return(PipelineAdjust::IncreasePipeline));
		EXPECT_CALL(*interestControl, markLowerLimit(5))
			.Times(1);
		EXPECT_CALL(*pp, segmentArrived(Name(threadPrefix)))
			.Times(1);
		ppc.segmentArrived(seg);
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
