//
//  video-playout-buffer.h
//  ndnrtc
//
//  Created by Peter Gusev on 12/9/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__video_playout_buffer__
#define __ndnrtc__video_playout_buffer__

#include "ndnrtc-common.h"
#include "playout-buffer.h"
#include "ndnrtc-utils.h"

namespace ndnrtc
{
#if 0 // old code
    /**
     * Video jitter buffer class
     */
    class VideoPlayoutBuffer : public PlayoutBuffer
    {
    public:
        shared_ptr<webrtc::EncodedImage>
        acquireNextFrame(FrameBuffer::Slot **slot = NULL,
                         bool incCounter = false);
        
        int releaseAcquiredFrame();
    };
#endif
}

#endif /* defined(__ndnrtc__video_playout_buffer__) */
