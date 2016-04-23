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
#include <boost/asio/steady_timer.hpp>

#include "media-stream-base.h"
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

	class VideoStreamImpl : public MediaStreamBase
	{
	public:
		VideoStreamImpl(const std::string& streamPrefix,
			const MediaStreamSettings& settings, bool useFec);
		~VideoStreamImpl();

		std::vector<std::string> getThreads() const;

		void incomingFrame(const ArgbRawFrameWrapper&);
		void incomingFrame(const I420RawFrameWrapper&);
		void setLogger(ndnlog::new_api::Logger*);
		
	private:
		friend LocalVideoStream::LocalVideoStream(const std::string&, const MediaStreamSettings&, bool);
		friend LocalVideoStream::~LocalVideoStream();

		VideoStreamImpl(const VideoStreamImpl& stream) = delete;

		class MetaKeeper : public MediaStreamBase::BaseMetaKeeper<VideoThreadMeta> {
		public:
			MetaKeeper(const VideoThreadParams* params);
			~MetaKeeper();

			VideoThreadMeta getMeta() const;
			double getRate() const;

			bool updateMeta(bool isKey, size_t nDataSeg, size_t nParitySeg);

		private:
			MetaKeeper(const MetaKeeper&) = delete;

			unsigned int rateId_, dId_, dpId_, kId_, kpId_;
		};

		bool fecEnabled_;
		RawFrameConverter conv_;
		std::map<std::string, boost::shared_ptr<VideoThread>> threads_;
		std::map<std::string, boost::shared_ptr<FrameScaler>> scalers_;
		std::map<std::string, boost::shared_ptr<MetaKeeper>> metaKeepers_;
		std::map<std::string, std::pair<uint64_t, uint64_t>> seqCounters_;
		uint64_t playbackCounter_;
		boost::shared_ptr<VideoPacketPublisher> publisher_;

		void add(const MediaThreadParams* params);
		void remove(const std::string& threadName);
		bool checkMeta();
		
		void feedFrame(const WebRtcVideoFrame& frame);
		void publish(std::map<std::string, boost::shared_ptr<VideoFramePacket>>& frames);
		void publish(const std::string& thread, boost::shared_ptr<VideoFramePacket>& fp);
		std::map<std::string, PacketNumber> getCurrentSyncList(bool forKey = false);
	};
}

#endif		