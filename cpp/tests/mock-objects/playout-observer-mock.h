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
#include "src/video-playout.h"

class MockPlayoutObserver : public ndnrtc::IPlayoutObserver
{
public:
	MOCK_METHOD0(onQueueEmpty, void(void));
};

class MockVideoPlayoutObserver : public ndnrtc::IVideoPlayoutObserver 
{
public:
	MOCK_METHOD0(onQueueEmpty, void(void));
	MOCK_METHOD2(frameSkipped, void(PacketNumber, bool));
	MOCK_METHOD2(frameProcessed, void(PacketNumber, bool));
	MOCK_METHOD2(recoveryFailure, void(PacketNumber, bool));
};

#endif