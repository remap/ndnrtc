// 
// playout-observer-mock.h
//
//  Created by Peter Gusev on 04 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playout_observer_mock_h__
#define __playout_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/playout.h"

class MockPlayoutObserver : public ndnrtc::IPlayoutObserver
{
public:
	MOCK_METHOD0(onQueueEmpty, void(void));
};

#endif