//
//  jitter-timing.h
//  ndnrtc
//
//  Created by Peter Gusev on 5/8/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__jitter_timing__
#define __ndnrtc__jitter_timing__

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "simple-log.h"
#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
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
    class JitterTiming : public new_api::NdnRtcComponent
    {
    public:
        JitterTiming();
        ~JitterTiming();
        
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
        void updatePlayoutTime(int framePlayoutTime, PacketNumber packetNo);
        
        /**
         * Sets up playback timer asynchronously.
         * @param callback A callback to call when timer is fired
         */
        void run(boost::function<void()> callback);
        
    private:
        int framePlayoutTimeMs_ = 0;
        int processingTimeUsec_ = 0;
        int64_t playoutTimestampUsec_ = 0;
        
        void resetData();
    };
}

#endif /* defined(__ndnrtc__jitter_timing__) */
