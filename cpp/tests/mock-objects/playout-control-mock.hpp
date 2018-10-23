// 
// playout-control-mock.hpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//


#ifndef __playout_control_mock_h__
#define __playout_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "playout-control.hpp"

class MockPlayoutControl : public ndnrtc::IPlayoutControl {
public:
	MOCK_METHOD2(allowPlayout, void(bool, int ffwd));
	MOCK_METHOD0(onNewSampleReady, void());
	MOCK_METHOD0(onQueueEmpty, void());
    MOCK_METHOD1(setThreshold, void(unsigned int));
    MOCK_CONST_METHOD0(getThreshold, unsigned int());
    MOCK_METHOD0(onDrdUpdate, void());
	MOCK_METHOD2(onCachedDrdUpdate, void(double, double));
	MOCK_METHOD2(onOriginalDrdUpdate, void(double, double));
};

#endif