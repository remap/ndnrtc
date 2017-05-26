//
//  jitter-timing.h
//  ndnrtc
//
//  Created by Peter Gusev on 5/8/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__jitter_timing__
#define __ndnrtc__jitter_timing__

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/function.hpp>

#include "ndnrtc-common.hpp"

namespace ndnlog {
    namespace new_api {
        class Logger;
    }
}

namespace ndnrtc {
    /**
     * Video jitter buffer timing class
     * Provides interface for managing playout timing in separate playout thread
     * Playout thread iteratively calls function which extracts frames from the
     * jitter buffer, renders them and sets a timer for the frame playout delay,
     * which is calculated from the timestamps, provided by producer and
     * adjusted by this class in order to accomodate processing delays
     * (extracting frame from the jitter buffer, rendering frame on the canvas,
     * etc.).
     */
    class JitterTimingImpl;
    class JitterTiming
    {
    public:
        JitterTiming(boost::asio::io_service& io);
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
        void updatePlayoutTime(int framePlayoutTime);
        
        /**
         * Sets up playback timer asynchronously.
         * @param callback A callback to call when timer is fired
         */
        void run(boost::function<void()> callback);

        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);
        void setDescription(const std::string& desc);
        
    private:
        JitterTiming(const JitterTiming&) = delete;

        boost::shared_ptr<JitterTimingImpl> pimpl_;
    };
}

#endif /* defined(__ndnrtc__jitter_timing__) */
