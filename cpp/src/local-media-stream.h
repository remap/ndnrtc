// 
// local-media-stream.h
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __local_media_stream_h__
#define __local_media_stream_h__

#include "interfaces.h"
#include "params.h"

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
	class MediaStreamSettings
	{
	public:
		MediaStreamSettings(boost::asio::io_service& faceIo,
			const MediaStreamParams& params):faceIo_(faceIo), params_(params){}
		~MediaStreamSettings(){}

		boost::asio::io_service& faceIo_;
		ndn::KeyChain* keyChain_;
		ndn::Face* face_;
		MediaStreamParams params_;
	};

	class VideoStreamImpl;
	
	class LocalVideoStream : public IExternalCapturer
	{
	public:
		LocalVideoStream(const std::string& streamPrefix, 
			const MediaStreamSettings& settings, bool useFec = true);
		~LocalVideoStream();

		void addThread(const VideoThreadParams* params);
		void removeThread(const std::string& threadName);

		int incomingArgbFrame(const unsigned int width,
			const unsigned int height,
			unsigned char* argbFrameData,
			unsigned int frameSize);

		int incomingI420Frame(const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideU,
			const unsigned int strideV,
			const unsigned char* yBuffer,
			const unsigned char* uBuffer,
			const unsigned char* vBuffer);

		std::string getPrefix() const;
		std::vector<std::string> getThreads() const;
		void setLogger(ndnlog::new_api::Logger*);

	private:
		LocalVideoStream(const LocalVideoStream&) = delete;
		LocalVideoStream(LocalVideoStream&&) = delete;

		boost::shared_ptr<VideoStreamImpl> pimpl_;
	};
}

#endif