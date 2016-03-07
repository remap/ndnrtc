// 
// client.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __client_h__
#define __client_h__

#include <stdlib.h>
#include <ndnrtc/interfaces.h>

#include "config.h"
#include "client-session-observer.h"
#include "remote-stream.h"

class Client {
public:
	static Client& getSharedInstance();

	void setLibraryObserver(ndnrtc::INdnRtcLibraryObserver& ndnrtcLibObserver);

	// blocking call. will return after runTimeSec seconds
	void run(unsigned int runTimeSec, unsigned int statSamplePeriodMs, 
		const ClientParams& params);

	~Client();
private:
	ndnrtc::INdnRtcLibrary *ndnp_;
	unsigned int runTimeSec_, statSampleIntervalMs_;
	ClientParams params_;
	ClientSessionObserver clientSessionObserver_;

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
