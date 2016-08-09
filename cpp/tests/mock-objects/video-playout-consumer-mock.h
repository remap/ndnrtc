// 
// video-playout-consumer-mock.h
//
//  Created by Peter Gusev on 03 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __video_playout_consumer_mock_h__
#define __video_playout_consumer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/video-playout.h"

class MockVideoPlayoutConsumer : public ndnrtc::IEncodedFrameConsumer
{
public:
	MOCK_METHOD1(processFrame, void(const webrtc::EncodedImage&));
};

#endif
