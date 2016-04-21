// 
// media-stream-base.h
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __media_stream_base_h__
#define __media_stream_base_h__

#include <boost/thread/mutex.hpp>
#include <ndn-cpp/name.hpp>

#include "local-media-stream.h"
#include "packet-publisher.h"

namespace ndn {
	class MemoryContentCache;
}

namespace ndnrtc {
	class MediaThreadParams;

	class MediaStreamBase : public NdnRtcComponent {
	public:
		MediaStreamBase(const std::string& basePrefix, 
			const MediaStreamSettings& settings);
		virtual ~MediaStreamBase();

		void addThread(const MediaThreadParams* params);
		void removeThread(const std::string& threadName);

		std::string getPrefix() { return streamPrefix_.toUri(); }
		virtual std::vector<std::string> getThreads() const = 0;

	protected:
		mutable boost::mutex internalMutex_;
		uint64_t metaVersion_;
		MediaStreamSettings settings_;
		ndn::Name streamPrefix_;
		boost::shared_ptr<ndn::MemoryContentCache> cache_;
		boost::shared_ptr<CommonPacketPublisher> dataPublisher_;

		virtual void add(const MediaThreadParams* params) = 0;
		virtual void remove(const std::string& threadName) = 0;
		void publishMeta();
	};
}

#endif
