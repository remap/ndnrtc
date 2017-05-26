// 
// test-webrtc-audio-channel.cc
//
//  Created by Peter Gusev on 17 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/webrtc-audio-channel.hpp"

using namespace ndnrtc;

TEST(TestWebRtcAudioChannel, TestCreate)
{
	WebrtcAudioChannel ac(WebrtcAudioChannel::Codec::Opus);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
