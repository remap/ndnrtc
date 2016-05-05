// 
// playback-queue-observer-mock.h
//
//  Created by Peter Gusev on 02 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playback_queue_observer_mock_h__
#define __playback_queue_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/frame-buffer.h"

class MockPlaybackQueueObserver : public ndnrtc::IPlaybackQueueObserver
{
public:
	MOCK_METHOD0(onNewSampleReady, void(void));
};

#endif
