// 
// stream.hpp
//
//  Created by Peter Gusev on 19 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __stream_h__
#define __stream_h__

#include "statistics.hpp"
#include "simple-log.hpp"

namespace ndnrtc {
	namespace statistics {
		class StatisticsStorage;
	}

    /**
     * Stream interface class used as a base class for remote and local streams.
     * Defines trivial methods common for both types of streams.
     */
	class IStream {
	public:
        /**
         * Returns base prefix for the stream
         */
		virtual std::string getBasePrefix() const = 0;
        
        /**
         * Returns stream name
         */
		virtual std::string getStreamName() const = 0;
		
        /**
         * Returns full stream for the stream used for fetching data
         */
        virtual std::string getPrefix() const = 0;
        
        /**
         * Returns statistics storage for the current stream.
         * Each stream has internal statistics storage with counters for 
         * various statistics. This call is non-blocking - user may invoke
         * this call at regular intervals to query running statistics.
         */
		virtual statistics::StatisticsStorage getStatistics() const = 0;
        
        /**
         * Sets logger for this stream. By default, logger is nil - no logging is
         * performed.
         */
		virtual void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger) = 0;
	};
}

#endif
