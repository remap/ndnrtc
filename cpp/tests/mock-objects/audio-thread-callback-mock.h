// 
// audio-thread-callback-mock.h
//
//  Created by Peter Gusev on 17 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __audio_thread_callback_mock_h__
#define __audio_thread_callback_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <src/audio-thread.h>

class MockAudioThreadCallback : public ndnrtc::IAudioThreadCallback
{
public:
	MOCK_METHOD1(onSampleBundle, void(ndnrtc::AudioBundlePacket& packet));
};

#endif