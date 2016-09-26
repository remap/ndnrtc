// 
// audio-sample-consumer-mock.hpp
//
//  Created by Peter Gusev on 17 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __audio_sample_consumer_mock_h__
#define __audio_sample_consumer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <src/audio-capturer.hpp>

class MockAudioSampleConsumer : public ndnrtc::new_api::IAudioSampleConsumer
{
public:
	MOCK_METHOD2(onDeliverRtpFrame, void(unsigned int, uint8_t*));
	MOCK_METHOD2(onDeliverRtcpFrame, void(unsigned int, uint8_t*));
};

#endif