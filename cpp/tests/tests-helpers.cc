// 
// tests-helpers.cc
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "tests-helpers.h"

using namespace ndnrtc::new_api;

VideoCoderParams sampleVideoCoderParams()
{
	VideoCoderParams vcp;

	vcp.codecFrameRate_ = 30;
	vcp.gop_ = 30;
	vcp.startBitrate_ = 1000;
	vcp.maxBitrate_ = 3000;
	vcp.encodeWidth_ = 1920;
	vcp.encodeHeight_ = 1080;
	vcp.dropFramesOn_ = true;

	return vcp;
}
