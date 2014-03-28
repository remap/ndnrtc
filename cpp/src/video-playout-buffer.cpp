//
//  video-playout-buffer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 12/9/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "video-playout-buffer.h"

//#define USE_AMP

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

#if 0
//******************************************************************************
//******************************************************************************
#pragma mark - public

shared_ptr<EncodedImage>
VideoPlayoutBuffer::acquireNextFrame(FrameBuffer::Slot **slot,
                                     bool incCounter)
{
    shared_ptr<EncodedImage> frame(nullptr);
    
    *slot = acquireNextSlot(incCounter);
    
    if (*slot)
        frame = (*slot)->getFrame();
    
    return frame;
}

int VideoPlayoutBuffer::releaseAcquiredFrame()
{
    return PlayoutBuffer::releaseAcquiredSlot();
}
#endif
