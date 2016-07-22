// 
// stream.h
//
//  Created by Peter Gusev on 19 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __stream_h__
#define __stream_h__

#include "statistics.h"
#include "simple-log.h"

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
		virtual void setLogger(ndnlog::new_api::Logger*) = 0;
	};
}

#endif