// 
// latency-control-mock.h
//
//  Created by Peter Gusev on 15 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __latency_control_mock_h__
#define __latency_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "latency-control.h"

class MockLatencyControl : public ndnrtc::ILatencyControl {
public:
	MOCK_METHOD0(reset, void());
	MOCK_CONST_METHOD0(getCurrentCommand, ndnrtc::PipelineAdjust());
};

#endif