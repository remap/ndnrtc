// 
// latency-control-observer-mock.hpp
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __latency_control_observer_mock_h__
#define __latency_control_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "latency-control.hpp"

class MockLatencyControlObserver : public ndnrtc::ILatencyControlObserver 
{
public:
	MOCK_METHOD1(needPipelineAdjustment, bool(const ndnrtc::PipelineAdjust&));
};

#endif