// 
// client.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __client_h__
#define __client_h__

#include <stdlib.h>
#include <boost/shared_ptr.hpp>
#include <ndnrtc/interfaces.h>

#include "config.h"
#include "client-session-observer.h"
#include "remote-stream.h"
#include "stat-collector.h"

class Client {
public: 
	static Client& getSharedInstance();

	// blocking call. will return after runTimeSec seconds
	void run(ndnrtc::INdnRtcLibrary* ndnp, unsigned int runTimeSec, 
		unsigned int statSamplePeriodMs, const ClientParams& params);

	~Client();

private:
	class LibraryObserver :  public ndnrtc::INdnRtcLibraryObserver {
		public:
		void onStateChanged(const char *state, const char *args) 
		{
		    LogInfo("") << "library state changed: " << state << "-" << args << std::endl;
		}

		void onErrorOccurred(int errorCode, const char *message) 
		{
		    LogError("") << "library returned error (" << errorCode << ") " << message << std::endl;
		}
	};

	boost::asio::io_service io_;
	ndnrtc::INdnRtcLibrary *ndnp_;
	LibraryObserver libObserver_;
	unsigned int runTimeSec_, statSampleIntervalMs_;
	ClientParams params_;
	ClientSessionObserver clientSessionObserver_;
	boost::shared_ptr<StatCollector> statCollector_;

	std::vector<RemoteStream> remoteStreams_;

	Client();
	Client(Client const&) = delete;
	void operator=(Client const&) = delete;

	void initSession();
	void setupConsumer();
	void setupProducer();
	void setupStatGathering();
	void runProcessLoop();
	void tearDownStatGathering();
	void tearDownProducer();
	void tearDownConsumer();

	RemoteStream initRemoteStream(const ConsumerStreamParams& p, 
		const ndnrtc::new_api::GeneralConsumerParams& generalParams);
};

#endif
