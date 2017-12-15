// 
// remote-stream.hpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_h__
#define __remote_stream_h__

#include <boost/asio.hpp>
#include "stream.hpp"

namespace ndn {
	class Face;
	class KeyChain;
}

namespace ndnrtc {
	class RemoteStreamImpl;
	class IRemoteStreamObserver;
    class IExternalRenderer;
    
    /**
     * Main class for handling remote streams - streams published by remote producers.
     * Provides interface to initiate/stop fetching remote streams.
     * Gives updates through callbacks on the observer object.
     * Audio stream plays back audio samples on the default audio device.
     * Video stream supplies decoded frame pixel buffers in callbacks on the supplied
     * IExternalRenderer object.
     * Upon creation, user must supply io_service object which will be used for all 
     * processing. User is responsible to call run() method on the supplied io_service 
     * object.
     * @see IStream interface for additional details
     */
	class RemoteStream : public IStream {
	public:
        // Stream states
		typedef enum _State {
			Chasing,        // stream performs "latest data chasing" - latency minimzation
			Adjusting,      // optimal fetching parameters are determined
			Fetching        // stream performs fetching of the latest media samples available
		} State;
        
        typedef enum _Event {
            Rebuffering,        // rebuffering occurred (normally, due to stream flow interruption)
            ThreadSwitched,     // stream completed switching to a new thread
            NewMeta,            // stream/thread metadata has been received (doesn't perform comparison
                                // between previously received meta and newly received - invoked every time
                                // meta arrives)
            StateUpdate,        // stream state has changed
            VerificationState   // stream verification status has changed - either stream data verification
                                // fails or recovers after failure; user should invoke isVerified()
        } Event;

        /**
         * Indicates, whether metadata was sucesfully fetched.
         * Upon creation, stream performs asynchronous metadata fetching. Stream can't start fetching
         * untill all metadata is fetched. Consequently, user may invoke start() before metadata is 
         * fetched. In this case, call is pended until metadata fetched completely and then, fetching 
         * is inititated. If user doesn't want to invoke start() before metadata is fetched, she must 
         * wait for the NewMeta event (see above) recevied by stream observer and/or repeatedly call 
         * this method to make sure meta is fetched.
         * @return true If metadata was fetched, false - otherwise
         */
		bool isMetaFetched() const;
        
        /**
         * Returns array of thread names for the current stream.
         * @return std::vector<std::string> object with thread names as vector elements
         * NOTE: returned vector is ALWAYS empty unless metadata is fully fetched (if isMetaFetched() 
         * returns false)
         */
		std::vector<std::string> getThreads() const;
        
        /**
         * Sets thread to fetch from.
         * If stream was already started, this will trigger switching to a new thread. Thread
         * switching is asynchronous and not instantaneous. User will be notified with ThreadSwitched
         * event upon completion.
         * @param threadName Name of the thread, that should be used for fetching media.
         */
		void setThread(const std::string& threadName);
        
        /**
         * Returns currently fetched thread name 
         */
        std::string getThread() const;
        
        /**
         * Stops fetching from this stream
         */
		void stop();
        
        /**
         * Sets interest lifetime for outgoing interests used for fetching data.
         * @param lifetimeMs Interest lifetime in milliseconds
         */
		void setInterestLifetime(unsigned int lifetimeMs);
        
        /**
         * Sets target playback buffer length this stream should try to maintain during fetching.
         * @param bufferSizeMs Length of the playback buffer in milliseconds
         */
		void setTargetBufferSize(unsigned int bufferSizeMs);

        /**
         * Indicates, whether last received data packet was verified succesfully.
         * User may monitor for VerificationState event for changes.
         */
		bool isVerified() const;

        /**
         * Indicates, whether stream is actively fetching data.
         */
        bool isRunning() const;
        
        /**
         * Registers an observer for this stream. Callbacks are always dispatched on 
         * the provided io_service's thread.
         * Stream can have multiple observers.
         */
        virtual void registerObserver(IRemoteStreamObserver* observer);
        
        /**
         * Unregistered stream observer, registered previously.
         */
        virtual void unregisterObserver(IRemoteStreamObserver* observer);

		statistics::StatisticsStorage getStatistics() const;
		void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

		std::string getPrefix() const { return streamPrefix_; }
		std::string getBasePrefix() const { return basePrefix_; }
		std::string getStreamName() const { return streamName_; }

	protected:
        /**
         * Initializes class
         * @param faceIo io_service object which will be used for all asynchronous events dispatching;
         *               all callbacks are dispatched using this object;
         *               user is responsible for calling run() method on this object;
         * @param face ndn::Face object used for issuing interests and receiving data objects
         * @param keyChain ndn::KeyChain object used for data verification - default certificate is used;
         * @param basePrefix Base prefix for the stream
         * @param streamName Stream name
         */
        RemoteStream(boost::asio::io_service& faceIo,
                     const boost::shared_ptr<ndn::Face>& face,
                     const boost::shared_ptr<ndn::KeyChain>& keyChain,
                     const std::string& basePrefix,
                     const std::string& streamName);
        virtual ~RemoteStream();
        
		std::string streamPrefix_, basePrefix_, streamName_;
		boost::shared_ptr<RemoteStreamImpl> pimpl_;
	};

    /**
     * Remote audio stream class used for fetching audio
     */
	class RemoteAudioStream: public RemoteStream {
	public:
		RemoteAudioStream(boost::asio::io_service& faceIo,
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName,
            const int interestLifeTime = 2000,
            const int jitterSizeMs = 150);

        /**
         * Starts fetching audio samples from the remote producer
         * @param threadName Thread name to fetch media from
         */
		void start(const std::string& threadName);
	};

    /**
     * Remote video stream class used for fetching video
     */
	class RemoteVideoStream: public RemoteStream {
	public:
		RemoteVideoStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName,
            const int interestLifeTime = 2000,
            const int jitterSizeMs = 150);

        /**
         * Starts fetching video frames from the remote producer
         * @param threadName Thread name to fetch media from
         * @param renderer Pointer to IExternalRenderer object which will receive callbacks with
         *                 decoded video frame pixel buffers.
         */
		void start(const std::string& threadName, 
			IExternalRenderer* renderer);
	};
    
    /**
     * Remote stream observer
     */
    class IRemoteStreamObserver {
    public:
        /**
         * Called every time new remote stream event has occurred.
         * User should expect that this callback is called on the same thread which invokes
         * run() on the io_service object, supplied to the stream upon its' creation.
         * @see RemoteStream constructor
         */
        virtual void onNewEvent(const RemoteStream::Event&) = 0;
    };
}

#endif
