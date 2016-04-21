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
#include <boost/thread/mutex.hpp>
#include <boost/asio/steady_timer.hpp>
#include <ndn-cpp/name.hpp>

#include "local-media-stream.h"
#include "ndnrtc-object.h"
#include "packet-publisher.h"
#include "frame-converter.h"

namespace ndn {
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
	class VideoThreadParams;

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
		friend LocalVideoStream::LocalVideoStream(const std::string&, const MediaStreamSettings&, bool);
		friend LocalVideoStream::~LocalVideoStream();

		VideoStreamImpl(const VideoStreamImpl& stream) = delete;

		class MetaKeeper {
		public:
			MetaKeeper(const VideoThreadParams* params);
			~MetaKeeper();

			bool updateMeta(bool isKey, size_t nDataSeg, size_t nParitySeg);
			VideoThreadMeta getMeta() const;
			bool isNewMetaAvailable() { bool f = newMeta_; newMeta_ = false; return f; }
			unsigned int getVersion() const { return metaVersion_; }
			void setVersion(unsigned int v) { metaVersion_ = v; }
			double getRate() const;

		private:
			MetaKeeper(const MetaKeeper&) = delete;

			bool newMeta_;
			const VideoThreadParams* params_;
			unsigned int metaVersion_;
			unsigned int rateId_, dId_, dpId_, kId_, kpId_;
		};

		bool fecEnabled_;
		ndn::Name streamPrefix_;
		MediaStreamSettings settings_;
		RawFrameConverter conv_;
		std::map<std::string, boost::shared_ptr<VideoThread>> threads_;
		std::map<std::string, boost::shared_ptr<FrameScaler>> scalers_;
		std::map<std::string, std::pair<uint64_t, uint64_t>> seqCounters_;
		std::map<std::string, boost::shared_ptr<MetaKeeper>> metaKeepers_;
		uint64_t playbackCounter_, metaVersion_;
		boost::shared_ptr<VideoPacketPublisher> publisher_;
		boost::shared_ptr<CommonPacketPublisher> dataPublisher_;
		boost::shared_ptr<ndn::MemoryContentCache> cache_;
		boost::asio::steady_timer metaCheckTimer_;
		boost::mutex internalMutex_;

		void add(const VideoThreadParams* params);
		void remove(const std::string& threadName);

		void feedFrame(const WebRtcVideoFrame& frame);
		void publish(std::map<std::string, boost::shared_ptr<VideoFramePacket>>& frames);
		void publish(const std::string& thread, boost::shared_ptr<VideoFramePacket>& fp);
		std::map<std::string, PacketNumber> getCurrentSyncList(bool forKey = false);
		void publishMeta();
		void setupMetaCheckTimer();
	};
}

#endif		