// 
// remote-stream-impl.h
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_impl_h__
#define __remote_stream_impl_h__

#include "remote-stream.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
	class SegmentController;
	class BufferControl;
	class PipelineControl;
	class SampleEstimator;
	class DrdEstimator;
	class PipelineControlStateMachine;
	class ILatencyControl;
	class IInterestControl;
	class IBuffer;
	class IPipeliner;
	class IInterestQueue;
	class IPlaybackQueue;
	class IPlayout;
	class IPlayoutControl;

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

		void start(const std::string& threadName);
		void setThread(const std::string& threadName);
		void stop();

		void setInterestLifetime(unsigned int lifetimeMs);
		void setTargetBufferSize(unsigned int bufferSizeMs);
		void setLogger(ndnlog::new_api::Logger* logger);

		void attach(IRemoteStreamObserver*);
		void detach(IRemoteStreamObserver*);

	protected:
		boost::asio::io_service& io_;
		std::vector<IRemoteStreamObserver*> observers_;
		boost::shared_ptr<PipelineControl> pipelineControl_;
		boost::shared_ptr<BufferControl> bufferControl_;
		boost::shared_ptr<IPlayoutControl> playoutControl_;
		boost::shared_ptr<IPipeliner> pipeliner_;
	};
}

#endif
