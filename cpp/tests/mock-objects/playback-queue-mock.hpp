// 
// playback-queue-mock.hpp
//
//  Created by Peter Gusev on 13 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playback_queue_h__
#define __playback_queue_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "frame-buffer.hpp"

class MockPlaybackQueue : public ndnrtc::IPlaybackQueue {
public:
	MOCK_METHOD1(pop, void(ndnrtc::ExtractSlot));
	MOCK_CONST_METHOD0(size, int64_t());
	MOCK_CONST_METHOD0(pendingSize, int64_t());
	MOCK_CONST_METHOD0(sampleRate, double());
	MOCK_CONST_METHOD0(samplePeriod, double());
    MOCK_METHOD1(attach, void(ndnrtc::IPlaybackQueueObserver*));
    MOCK_METHOD1(detach, void(ndnrtc::IPlaybackQueueObserver*));
};

#endif