// 
// video-stream-impl.h
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __video_stream_impl_h__
#define __video_stream_impl_h__

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <ndn-cpp/name.hpp>

#include "local-media-stream.h"
#include "ndnrtc-object.h"
#include "packet-publisher.h"
#include "frame-converter.h"

namespace ndn {
	class KeyChain;
	class Face;
	class MemoryContentCache;
}

namespace ndnlog{
	namespace new_api{
		class Logger;
	}
}

namespace ndnrtc {
	class VideoThread;
	class FrameScaler;
	class VideoFramePacket;

	class VideoStreamImpl : public NdnRtcComponent
	{
	public:
		VideoStreamImpl(const std::string& streamPrefix,
			const MediaStreamSettings& settings, bool useFec);
		~VideoStreamImpl();

		void addThread(const VideoThreadParams* params);
		void removeThread(const std::string& threadName);

		std::string getPrefix();
		std::vector<std::string> getThreads();

		void incomingFrame(const ArgbRawFrameWrapper&);
		void incomingFrame(const I420RawFrameWrapper&);
		void setLogger(ndnlog::new_api::Logger*);
		
	private:
		VideoStreamImpl(const VideoStreamImpl& stream) = delete;

		bool fecEnabled_;
		ndn::Name streamPrefix_;
		MediaStreamSettings settings_;
		RawFrameConverter conv_;
		std::map<std::string, boost::shared_ptr<VideoThread>> threads_;
		std::map<std::string, boost::shared_ptr<FrameScaler>> scalers_;
		std::map<std::string, std::pair<uint64_t, uint64_t>> seqCounters_;
		std::map<std::string, unsigned int> rateEstimators_;
		uint64_t playbackCounter_;
		boost::shared_ptr<VideoPacketPublisher> publisher_;
		boost::shared_ptr<CommonPacketPublisher> parityPublisher_;
		boost::shared_ptr<ndn::MemoryContentCache> cache_;

		void feedFrame(const WebRtcVideoFrame& frame);
		void publish(std::map<std::string, boost::shared_ptr<VideoFramePacket>>& frames);
		void publish(const std::string& thread, boost::shared_ptr<VideoFramePacket>& fp);
		std::map<std::string, PacketNumber> getCurrentSyncList(bool forKey = false);
	};
}

#endif		