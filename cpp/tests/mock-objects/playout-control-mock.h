// 
// playout-control-mock.h
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//


#ifndef __playout_control_mock_h__
#define __playout_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "playout-control.h"

class MockPlayoutControl : public ndnrtc::IPlayoutControl {
public:
	MOCK_METHOD1(allowPlayout, void(bool));
	MOCK_METHOD0(onNewSampleReady, void());
	MOCK_METHOD0(onQueueEmpty, void());
};

#endif