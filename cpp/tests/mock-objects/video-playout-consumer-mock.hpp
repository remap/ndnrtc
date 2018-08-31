// 
// video-playout-consumer-mock.hpp
//
//  Created by Peter Gusev on 03 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __video_playout_consumer_mock_h__
#define __video_playout_consumer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/video-playout.hpp"

class MockVideoPlayoutConsumer : public ndnrtc::IEncodedFrameConsumer
{
public:
	MOCK_METHOD2(processFrame, void(const ndnrtc::FrameInfo&, const webrtc::EncodedImage&));
};

#endif
