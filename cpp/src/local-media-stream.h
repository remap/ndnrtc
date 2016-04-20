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
	/**
	 * Media stream settings class unites objects, required for media streams
	 * creation and operation
	 */
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
	
	/**
	 * LocalVideoStream shall be used for publishing video. It provides interface for 
	 * feeding raw ARGB or I420 frames. These frames will be encoded asynchronously 
	 * for different bitrates (depedning on added video threads that represent encoder
	 * instances) and then added to internal memory content cache. Access to internal 
	 * content cache is synchronized with Face thread, which is represented by io_service
	 * object passed to LiveVideoStream at construction.
	 * User should also pass pointers to Face and KeyChain objects. User is responsbile 
	 * for running Face io_service.
	 * For more info on thread-safety and thread-safe faces:
	 * @see boost::asio::io_service
	 * @see ndn::ThreadsafeFace
	 */
	class LocalVideoStream : public IExternalCapturer
	{
	public:
		/**
		 * This constructs LocalVideoStream object.
		 * Throws in several cases:
		 * - bad MediaStreamParameters type (MediaStreamTypeAudio) in settings
		 * - some threads in params_ have identical names
		 * @param basePrefix base prefix used for constructing stream prefix and 
		 *		derived data packet prefixes
		 * @param settings Stream settings structure
		 * @param useFec Indicates, whether Forward Error Correction should be used 
		 *		and parity data should be published
		 */
		LocalVideoStream(const std::string& basePrefix, 
			const MediaStreamSettings& settings, bool useFec = true);
		~LocalVideoStream();

		/**
		 * Add video thread
		 * @param params Video thread parameters
		 */
		void addThread(const VideoThreadParams* params);

		/**
		 * Remove video thread
		 * @param threadName Thread name to remove
		 */
		void removeThread(const std::string& threadName);

		/**
		 * Encode and publish ARGB frame data.
		 * This initiates encoding of raw frames for each video thread and
		 * publishes encoded data according to NDN-RTC namespace. 
		 * Call is asynchronous: returns immediately. Publishing is performed
		 * on Face thread to avoid data races.
		 */
		int incomingArgbFrame(const unsigned int width,
			const unsigned int height,
			unsigned char* argbFrameData,
			unsigned int frameSize);

		/**
		 * Encode and publish I420 frame data.
		 * This initiates encoding of raw frames for each video thread and
		 * publishes encoded data according to NDN-RTC namespace. 
		 * Call is asynchronous: returns immediately. Publishing is performed
		 * on Face thread to avoid data races.
		 */
		int incomingI420Frame(const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideU,
			const unsigned int strideV,
			const unsigned char* yBuffer,
			const unsigned char* uBuffer,
			const unsigned char* vBuffer);

		/**
		 * Returns full stream prefix used for publishing data
		 * @return Full stream prefix
		 */
		std::string getPrefix() const;
		
		/**
		 * Returns array of current video thread names
		 * @return Vector of current video thread names
		 */
		std::vector<std::string> getThreads() const;

		/**
		 * Sets logger for current stream
		 * @param logger Pointer to Logger instance
		 */
		void setLogger(ndnlog::new_api::Logger* logger);

	private:
		LocalVideoStream(const LocalVideoStream&) = delete;
		LocalVideoStream(LocalVideoStream&&) = delete;

		boost::shared_ptr<VideoStreamImpl> pimpl_;
	};
}

#endif