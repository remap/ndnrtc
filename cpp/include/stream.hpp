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

	class IStream {
	public:
		virtual std::string getBasePrefix() const = 0;
		virtual std::string getStreamName() const = 0;
		virtual std::string getPrefix() const = 0;
		virtual statistics::StatisticsStorage getStatistics() const = 0;
		virtual void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger) = 0;
	};
}

#endif