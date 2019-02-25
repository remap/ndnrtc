//
// tools.hpp
//
//  Created by Peter Gusev on 29 October 2018.
//  Copyright 2013-2019 Regents of the University of California
//

#ifndef __tools_hpp__
#define __tools_hpp__

#include "../../src/video-codec.hpp"

namespace ndnrtc {

int writeFrame(FILE* file, const VideoCodec::EncodedFrame& frame);
int readFrame(VideoCodec::EncodedFrame& frame, FILE *file);

}
#endif
