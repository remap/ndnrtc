// 
// audio-stream-impl.hpp
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __audio_stream_h__
#define __audio_stream_h__

#include "media-stream-base.hpp"
#include "audio-thread.hpp"

namespace ndnrtc {
	class AudioThread;
	class AudiobundlePacket;

	class AudioStreamImpl : public MediaStreamBase,
							public IAudioThreadCallback {
	public:
		AudioStreamImpl(const std::string& basePrefix,
			const MediaStreamSettings& settings);
		~AudioStreamImpl();

		// may be called from main thread or face thread
		void start();
		void stop();

		bool isRunning() const { return streamRunning_; };
		std::vector<std::string> getThreads() const;
		void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

	private:
		friend LocalAudioStream::LocalAudioStream(const std::string& basePrefix,
			const MediaStreamSettings& settings);

		AudioStreamImpl(const AudioStreamImpl&) = delete;

		class MetaKeeper : public MediaStreamBase::BaseMetaKeeper<AudioThreadMeta> {
		public:
			MetaKeeper(const AudioThreadParams* params):BaseMetaKeeper(params), 
				rate_(0){}
			~MetaKeeper(){}

			double getRate() const { return rate_; }
			bool updateMeta(double rate);
			AudioThreadMeta getMeta() const;

		private:
			MetaKeeper(const MetaKeeper&) = delete;

			double rate_;
		};

        boost::shared_ptr<CommonPacketPublisher> samplePublisher_;
		std::map<std::string, boost::shared_ptr<AudioThread>> threads_;
		std::map<std::string, boost::shared_ptr<MetaKeeper>> metaKeepers_;
		std::vector<boost::shared_ptr<AudioBundlePacket>> bundlePool_;
		boost::atomic<bool> streamRunning_;

		// may be called from main thread or face thread
		void add(const MediaThreadParams* params);
		void remove(const std::string& threadName);

		// called on audio thread. need to sync with Face thread and 
		// on access to internal data, if any
		void onSampleBundle(std::string threadName, uint64_t bundleNo,
			boost::shared_ptr<AudioBundlePacket> packet);

		bool updateMeta();
	};
}

#endif
