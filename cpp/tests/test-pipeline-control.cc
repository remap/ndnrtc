//
// test-pipeline-control.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/pipeline-control.hpp"

using namespace ::testing;
using namespace std;
using namespace ndnrtc::v4;

TEST(TestPipelineControl, TestFixedW)
{
    int w = 8;
    PipelineControl ppCtrl(w);

    EXPECT_EQ(w, ppCtrl.getWSize());

    int nNewRequest = 0, nSkip = 0;
    ppCtrl.onNewRequest.connect([&](){
        nNewRequest++;
    });
    ppCtrl.onSkipPulse.connect([&](){
        nSkip++;
    });

    ppCtrl.pulse();
    EXPECT_EQ(nNewRequest, w);
    EXPECT_EQ(nSkip, 0);

    w /= 2;
    ppCtrl.setWSize(w);

    nNewRequest = 0; nSkip = 0;
    for (int i = 0; i < w; ++i)
        ppCtrl.pulse();

    EXPECT_EQ(nNewRequest, 0);
    EXPECT_EQ(nSkip, w);

    for (int i = 0; i < w; ++i)
        ppCtrl.pulse();
    EXPECT_EQ(nNewRequest, w);
    EXPECT_EQ(nSkip, w);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
