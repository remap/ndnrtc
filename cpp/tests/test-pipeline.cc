//
// test-pipeline-control.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/pipeline-control.hpp"
#include "src/pipeline.hpp"
#include "include/name-components.hpp"

#include <ndn-cpp/name.hpp>
#include <ndn-cpp/interest.hpp>

using namespace ::testing;
using namespace std;
using namespace ndn;
using namespace ndnrtc;

TEST(TestPipeline, TestDefault)
{
    Name prefix("/my/stream/prefix");
    Pipeline pp(prefix, 0);

    int nSlots = 0;
    pp.onNewSlot.connect([&](boost::shared_ptr<Pipeline::ISlot> slot){
        nSlots++;

        bool hasMeta = false, hasManifest = false, nPayloadSeg = 0, nParitySeg = 0;
        std::vector<boost::shared_ptr<const ndn::Interest>> outstandingI =
            slot->getOutstandingInterests();

        for (auto i:outstandingI)
        {
            if (i->getName()[-1].toEscapedString() == NameComponents::NameComponentMeta)
                    hasMeta = true;
            if (i->getName()[-1].toEscapedString() == NameComponents::NameComponentManifest)
                hasManifest = true;
            if (i->getName()[-1].isSegment())
            {
                if (i->getName()[-2].toEscapedString() == NameComponents::NameComponentParity)
                    nParitySeg++;
                if (i->getName()[-2].isSequenceNumber())
                    nPayloadSeg++;
            }
        }

        EXPECT_TRUE(hasMeta);
        EXPECT_TRUE(hasManifest);
        EXPECT_GT(nPayloadSeg, 0);
        EXPECT_GT(nParitySeg, 0);
    });

    pp.pulse();
    EXPECT_EQ(nSlots, 1);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
