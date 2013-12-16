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
    // how fast playout should slow compared to the jitter buffer decrease
    const double PlayoutJitterRatio = 1/3;
    const double ExtraTimePerFrame = 0.3;
    
    class VideoPlayoutBuffer : public PlayoutBuffer
    {
    public:
        shared_ptr<webrtc::EncodedImage>
        acquireNextFrame(FrameBuffer::Slot **slot = NULL,
                         bool incCounter = false);
        
        int releaseAcquiredFrame();
        int init(FrameBuffer *buffer,
                 unsigned int jitterSize = 1,
                 double startFrameRate = 30);
        
    private:
        bool nextFramePresent_ = true;
        int currentPlayoutTimeMs_, adaptedPlayoutTimeMs_;
        uint64_t lastFrameTimestampMs_ = 0;
        
        // Adaptive Media Playback parameters
        int ampExtraTimeMs_ = 0;    // whenever playback is slowed down, this
                                    // variable tracks cummulative amount of
                                    // milliseconds added for each playout
                                    // adaptation
        
        int ampThreshold_ = 10;     // size of the jitter buffer which is taken
                                    // as a boundary for enabling of the AMP
                                    // algorithm
        
        // Adaptive Media Playout mechanism when jitter size less than
        // ampThreshold_
        int getAdaptedPlayoutTime(int playoutTimeMs, int jitterSize);
    };
}

#endif /* defined(__ndnrtc__video_playout_buffer__) */
