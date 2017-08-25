// 
// latency-control-mock.hpp
//
//  Created by Peter Gusev on 15 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __latency_control_mock_h__
#define __latency_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "latency-control.hpp"

class MockLatencyControl : public ndnrtc::ILatencyControl {
public:
	MOCK_METHOD0(reset, void());
	MOCK_CONST_METHOD0(getCurrentCommand, ndnrtc::PipelineAdjust());
    MOCK_METHOD1(registerObserver, void(ndnrtc::ILatencyControlObserver*));
    MOCK_METHOD0(unregisterObserver, void());
    MOCK_METHOD1(setPlayoutControl, void(boost::shared_ptr<ndnrtc::IPlayoutControl>));
};

#endif