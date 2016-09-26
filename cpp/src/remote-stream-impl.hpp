// 
// remote-stream-impl.hpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_impl_h__
#define __remote_stream_impl_h__

#include "remote-stream.hpp"
#include "ndnrtc-object.hpp"
#include "meta-fetcher.hpp"
#include "data-validator.hpp"

namespace ndnrtc {
	namespace statistics {
		class StatisticsStorage;
	}

	class SegmentController;
	class BufferControl;
	class PipelineControl;
	class SampleEstimator;
	class DrdEstimator;
	class PipelineControlStateMachine;
    class PipelineControl;
	class ILatencyControl;
	class IInterestControl;
	class IBuffer;
	class IPipeliner;
	class IInterestQueue;
	class IPlaybackQueue;
	class IPlayout;
	class IPlayoutControl;
	class MediaStreamMeta;

	/**
	 * RemoteStreamImpl is a base class for implementing remote stream functionality
	 */
	class RemoteStreamImpl : public NdnRtcComponent {
	public:
		RemoteStreamImpl(boost::asio::io_service& io, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix);
	
		bool isMetaFetched() const;
		std::vector<std::string> getThreads() const;

		void fetchMeta();
		void start(const std::string& threadName);
		void setThread(const std::string& threadName);
		void stop();

		void setInterestLifetime(unsigned int lifetimeMs);
		void setTargetBufferSize(unsigned int bufferSizeMs);
		void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

		bool isVerified() const;
		// void attach(IRemoteStreamObserver*);
		// void detach(IRemoteStreamObserver*);

		void setNeedsMeta(bool needMeta) { needMeta_ = needMeta; }
		statistics::StatisticsStorage getStatistics() const;

	protected:
        MediaStreamParams::MediaStreamType type_;
		bool needMeta_, isRunning_, cuedToRun_;
		boost::shared_ptr<ndn::Face> face_;
		boost::shared_ptr<ndn::KeyChain> keyChain_;
		std::string streamPrefix_, threadName_;
		boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

		std::vector<IRemoteStreamObserver*> observers_;
		boost::shared_ptr<MetaFetcher> metaFetcher_;
		boost::shared_ptr<MediaStreamMeta> streamMeta_;
		std::map<std::string, boost::shared_ptr<NetworkData>> threadsMeta_;

        boost::shared_ptr<IBuffer> buffer_;
        boost::shared_ptr<SegmentController> segmentController_;
        boost::shared_ptr<PipelineControl> pipelineControl_;
		boost::shared_ptr<BufferControl> bufferControl_;
        boost::shared_ptr<SampleEstimator> sampleEstimator_;
		boost::shared_ptr<IPlayoutControl> playoutControl_;
        boost::shared_ptr<IInterestQueue> interestQueue_;
		boost::shared_ptr<IPipeliner> pipeliner_;
		boost::shared_ptr<ILatencyControl> latencyControl_;
		boost::shared_ptr<IInterestControl> interestControl_;
		boost::shared_ptr<IPlayout> playout_;
		boost::shared_ptr<IPlaybackQueue> playbackQueue_;

		std::vector<ValidationErrorInfo> validationInfo_;

		void fetchThreadMeta(const std::string& threadName);
		void streamMetaFetched(NetworkData&);
		void threadMetaFetched(const std::string& thread, NetworkData&);
		virtual void initiateFetching();
		virtual void stopFetching();
		void addValidationInfo(const std::vector<ValidationErrorInfo>&);
	};
}

#endif
