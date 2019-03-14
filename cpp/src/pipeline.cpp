//
//  pipeline.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 13 March 2019.
//  Copyright 2019 Regents of the University of California
//

#include "pipeline.hpp"

#include "interest-queue.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;

Pipeline::Pipeline(const ndn::Name &sequencePrefix, uint32_t nextSeqNo)
    : sequencePrefix_(sequencePrefix)
    , nextSeqNo_(nextSeqNo)
{

}


Pipeline::~Pipeline()
{

}

void
Pipeline::pulse()
{

}
