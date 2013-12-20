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
    
    /**
     * Video jitter buffer timing class
     * Provides interface for managing playout timing in separate playout thread
     * Playout thread itratively calls function which extracts frames from the 
     * jitter buffer, renders them and sets a timer for the frame playout delay,
     * which is calculated from the timestamps, provided by producer and 
     * adjusted by this class in order to accomodate processing delays 
     * (extracting frame from the jitter buffer, rendering frame on the canvas,
     * etc.).
     */
    class VideoJitterTiming
    {
    public:
        VideoJitterTiming();
        ~VideoJitterTiming(){}
        
        void flush();
        void stop();
        
        /**
         * Should be called in the beginning of the each playout iteration
         * @return current time in microseconds
         */
        int64_t startFramePlayout();
        
        /**
         * Should be called whenever playout time (as provided by producer) is 
         * known by the consumer. Usually, jitter buffer provides this value as 
         * a result of releaseAcqiuredFrame call.
         * @param framePlayoutTime Playout time meant by producer (difference 
         *                         between conqequent frame's timestamps)
         */
        void updatePlayoutTime(int framePlayoutTime);
        
        /**
         * Schedules and runs playout timer for current calculated playout time
         */
        void runPlayoutTimer();
        
    private:
        webrtc::EventWrapper &playoutTimer_;
        
        int framePlayoutTimeMs_ = 0;
        int processingTimeUsec_ = 0;
        int64_t playoutTimestampUsec_ = 0;
    };
    
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
