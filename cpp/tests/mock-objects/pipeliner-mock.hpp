// 
// pipeliner-mock.hpp
//
//  Created by Peter Gusev on 14 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeliner_mock_h__
#define __pipeliner_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "pipeliner.hpp"

class MockPipeliner : public ndnrtc::IPipeliner {
public:
    MOCK_METHOD2(express, void(const ndn::Name&, bool));
    MOCK_METHOD2(express, void(const std::vector<boost::shared_ptr<const ndn::Interest>>&, bool));
    MOCK_METHOD1(fillUpPipeline, void(const ndn::Name&));
    MOCK_METHOD0(reset, void());
    MOCK_METHOD1(setNeedSample, void(ndnrtc::SampleClass));
    MOCK_METHOD0(setNeedMetadata, void());
    MOCK_METHOD2(setSequenceNumber, void(PacketNumber seqNo, ndnrtc::SampleClass cls));
    MOCK_METHOD1(getSequenceNumber, PacketNumber(ndnrtc::SampleClass));
    MOCK_METHOD1(setInterestLifetime, void(unsigned int));
};

#endif