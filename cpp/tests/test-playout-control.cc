//
// test-playout-control.cc
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "playout-control.hpp"

#include "mock-objects/playback-queue-mock.hpp"
#include "mock-objects/playout-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;
#if 0
TEST(TestPlayoutControl, TestDefault)
{
    boost::shared_ptr<MockPlayout> playout(boost::make_shared<MockPlayout>());
    boost::shared_ptr<MockPlaybackQueue> playbackQueue(boost::make_shared<MockPlaybackQueue>());
    PlayoutControl pc(playout, playbackQueue, 150);

    {
        EXPECT_CALL(*playbackQueue, size())
            .Times(1)
            .WillOnce(Return(200));
        EXPECT_CALL(*playout, isRunning())
            .Times(8)
            .WillRepeatedly(Return(false));
        EXPECT_CALL(*playout, start(50))
            .Times(1);

        for (int i = 0; i < 7; ++i)
            pc.onNewSampleReady();

        pc.allowPlayout(true);

        EXPECT_CALL(*playout, stop())
            .Times(1);
        EXPECT_CALL(*playout, isRunning())
            .Times(3)
            .WillOnce(Return(true))
            .WillOnce(Return(false))
            .WillOnce(Return(false));

        pc.allowPlayout(false);
        pc.allowPlayout(false);
        pc.allowPlayout(false);
    }

    {
        EXPECT_CALL(*playbackQueue, size())
            .Times(6) // note this number
            .WillOnce(Return(0))
            .WillOnce(Return(50))
            .WillOnce(Return(75))
            .WillOnce(Return(100))
            .WillOnce(Return(125))
            .WillOnce(Return(150));
        EXPECT_CALL(*playout, start(0))
            .Times(1);
        EXPECT_CALL(*playout, isRunning())
            .Times(8)
            .WillOnce(Return(false))
            .WillOnce(Return(false))
            .WillOnce(Return(false))
            .WillOnce(Return(false))
            .WillOnce(Return(false))
            .WillOnce(Return(false))
            .WillRepeatedly(Return(true));

        pc.allowPlayout(true);

        for (int i = 0; i < 7; ++i)
            pc.onNewSampleReady();
    }
}
#endif
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
