// 
// audio-thread-callback-mock.hpp
//
//  Created by Peter Gusev on 17 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __audio_thread_callback_mock_h__
#define __audio_thread_callback_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <src/audio-thread.hpp>

class MockAudioThreadCallback : public ndnrtc::IAudioThreadCallback
{
public:
	MOCK_METHOD3(onSampleBundle, void(std::string, uint64_t, boost::shared_ptr<ndnrtc::AudioBundlePacket>));
};

#endif