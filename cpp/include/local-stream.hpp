// 
// local-stream.hpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __local_stream_h__
#define __local_stream_h__

#include "interfaces.hpp"
#include "params.hpp"
#include "stream.hpp"

namespace ndn {
	class KeyChain;
	class Face;
	class MemoryContentCache;
}

namespace ndnlog {
	namespace new_api{
		class Logger;
	}
}

namespace boost {
	namespace asio {
		class io_service;
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
			const MediaStreamParams& params):sign_(true), faceIo_(faceIo), params_(params){}
		~MediaStreamSettings(){}

        bool sign_;
		boost::asio::io_service& faceIo_;
		ndn::KeyChain* keyChain_;
		ndn::Face* face_;
		MediaStreamParams params_;
	};

	class VideoStreamImpl;
	class AudioStreamImpl;

	class LocalAudioStream : public IStream {
	public:
		LocalAudioStream(const std::string& basePrefix, 
			const MediaStreamSettings& settings);
		~LocalAudioStream();

		void start();
		void stop();

		void addThread(const AudioThreadParams params);
		void removeThread(const std::string& thread);

		bool isRunning() const;

		/**
		 * Returns full stream prefix used for publishing data
		 * @return Full stream prefix
		 */
		std::string getPrefix() const;
		std::string getBasePrefix() const;
		std::string getStreamName() const;
		
		/**
		 * Returns array of current video thread names
		 * @return Vector of current video thread names
		 */
		std::vector<std::string> getThreads() const;

		/**
		 * Sets logger for current stream
		 * @param logger Pointer to Logger instance
		 */
		void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

		/**
		 * Returns statistics storage for this stream
		 */
		statistics::StatisticsStorage getStatistics() const;

		static std::vector<std::pair<std::string, std::string>> getRecordingDevices();
		static std::vector<std::pair<std::string, std::string>> getPlayoutDevices();

	private:
		LocalAudioStream(const LocalAudioStream&) = delete;
		LocalAudioStream(LocalAudioStream&&) = delete;
		
		boost::shared_ptr<AudioStreamImpl> pimpl_;
	};

	/**
	 * LocalVideoStream shall be used for publishing video. It provides interface for 
	 * feeding raw ARGB or I420 frames. These frames will be encoded asynchronously 
	 * for different bitrates (depedning on added video threads that represent encoder
	 * instances) and then added to internal memory content cache. Access to internal 
	 * content cache is synchronized with Face thread, which is represented by io_service
	 * object passed to LiveVideoStream at construction. Stream also publishes meta 
	 * information about itself under <streamPrefix>/_meta namespace and meta information
	 * about each thread under <streamPrefix>/<threadName>/_meta namespace. This meta 
	 * objects contain information necessary for consumer to initialize it's structures
	 * in order to properly decode fetched data.
	 *
	 * User should also pass pointers to Face and KeyChain objects. User is responsbile 
	 * for running Face io_service.
	 * For more info on thread-safety and thread-safe faces:
	 * @see boost::asio::io_service
	 * @see ndn::ThreadsafeFace
	 *
	 * NOTE ON THREADING: Usually, one would create an instance of LocalVideoStream on 
	 * main application thread. Then, frames could be captured on some other thread 
	 * (other than main application thread - capturing thread). Finally, there is a 
	 * Face thread, represented by io_service passed at construction time. In this 
	 * general case, there are three separate threads: main, capture and face (most 
	 * applications, however, will have capturing callbacks on the main thread). 
	 * Current LocalVideoStream implementation supports this general setup and works
	 * properly if:
	 *  - construction/destruction called on main thread
	 *  - adding/removing threads called on main or capture thread
	 *  - incomingArgbFrame/incomingI420Frame called on capture thread
	 *  - setLogger/getThreads/getPrefix called on main or capture thread
	 * Consequently, LocalVideoStream ensures that any access to Face/KeyChain is 
	 * performed on the face thread.
	 */
	class LocalVideoStream : public IStream, public IExternalCapturer
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
		void addThread(const VideoThreadParams params);

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
			unsigned int frameSize,
			unsigned int userDataSize = 0,
			unsigned char* userData = nullptr);

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
			const unsigned char* vBuffer,
			unsigned int userDataSize = 0,
			unsigned char* userData = nullptr);

		/**
		 * Returns full stream prefix used for publishing data
		 * @return Full stream prefix
		 */
		std::string getPrefix() const;
		std::string getBasePrefix() const;
		std::string getStreamName() const;
		
		/**
		 * Returns array of current video thread names
		 * @return Vector of current video thread names
		 */
		std::vector<std::string> getThreads() const;

		/**
		 * Sets logger for current stream
		 * @param logger Pointer to Logger instance
		 */
		void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

		/**
		 * Returns statistics storage for this stream
		 */
		statistics::StatisticsStorage getStatistics() const;

	private:
		LocalVideoStream(const LocalVideoStream&) = delete;
		LocalVideoStream(LocalVideoStream&&) = delete;

		boost::shared_ptr<VideoStreamImpl> pimpl_;
	};
}

#endif
