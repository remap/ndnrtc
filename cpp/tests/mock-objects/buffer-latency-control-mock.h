// 
// buffer-latency-control-mock.h
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_latency_control_mock_h__
#define __buffer_latency_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "buffer-control.h"

class MockBufferLatencyControl : public ndnrtc::IBufferLatencyControl
{
public:
	MOCK_METHOD1(setTargetRate, void(double));
	MOCK_METHOD0(sampleArrived, void(void));
};

#endif