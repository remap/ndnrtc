// 
// buffer-control-observer-mock.hpp
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_control_observer_mock_h__
#define __buffer_control_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "buffer-control.hpp"

class MockBufferControlObserver : public ndnrtc::IBufferControlObserver
{
public:
	MOCK_METHOD1(targetRateUpdate, void(double));
	MOCK_METHOD1(sampleArrived, void(const PacketNumber&));
};

#endif