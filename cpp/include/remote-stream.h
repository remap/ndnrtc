// 
// remote-stream.h
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_h__
#define __remote_stream_h__

#include <boost/asio.hpp>
#include "stream.h"

namespace ndn {
	class Face;
	class KeyChain;
}

namespace ndnrtc {
	class RemoteStreamImpl;
	class IRemoteStreamObserver;

	class RemoteStream : public IStream {
	public:
		typedef enum _State {
			Chasing,
			Adjusting,
			Fetching
		} State;

		RemoteStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName);
		virtual ~RemoteStream();

		bool isMetaFetched() const;
		std::vector<std::string> getThreads() const;

		void start(const std::string& threadName);
		void setThread(const std::string& threadName);
		void stop();
		void setInterestLifetime(unsigned int lifetimeMs);
		void setTargetBufferSize(unsigned int bufferSizeMs);

		bool isVerified() const;

		/**
		 * Returns current statistics storage.
		 */
		statistics::StatisticsStorage getStatistics() const;

		/**
		 * Sets logger for current stream
		 * @param logger Pointer to Logger instance
		 */
		void setLogger(ndnlog::new_api::Logger* logger);

		std::string getPrefix() const { return streamPrefix_; }
		std::string getBasePrefix() const { return basePrefix_; }
		std::string getStreamName() const { return streamName_; }

	protected:
		std::string streamPrefix_, basePrefix_, streamName_;
		boost::shared_ptr<RemoteStreamImpl> pimpl_;
	};

	class RemoteAudioStream: public RemoteStream {
	public:
		RemoteAudioStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName);
	};

	class RemoteVideoStream: public RemoteStream {
	public:
		RemoteVideoStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName);
	};
}

#endif
