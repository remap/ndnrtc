// 
// remote-stream.h
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_h__
#define __remote_stream_h__

#include <boost/asio.hpp>

namespace ndn {
	class Face;
	class KeyChain;
}

namespace ndnrtc {
	class RemoteStreamImpl;
	class IRemoteStreamObserver;

	class RemoteStream {
	public:
		typedef enum _State {
			Chasing,
			Adjusting,
			Fetching
		} State;

		RemoteStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix);
		~RemoteStream();

		bool isMetaFetched() const;
		std::vector<std::string> getThreads() const;

		void start(const std::string& threadName);
		void setThread(const std::string& threadName);
		void stop();
		void setInterestLifetime(unsigned int lifetimeMs);
		void setTargetBufferSize(unsigned int bufferSizeMs);

		void attach(IRemoteStreamObserver*);
		void detach(IRemoteStreamObserver*);

		// static boost::shared_ptr<RemoteStream> 
			// createStream(const std::string streamPrefix);
	private:
		boost::shared_ptr<RemoteStreamImpl> pimpl_;
	};

	class IRemoteStreamObserver {
	public:
		virtual void onRebuffering() = 0;
		virtual void onSkippedFrame() = 0;
		virtual void onStateChange(RemoteStream::State) = 0;
	};
}

#endif
