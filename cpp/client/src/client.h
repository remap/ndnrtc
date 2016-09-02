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
#include "stream.h"
#include "stat-collector.h"

namespace ndn {
	class KeyChain;
}

class Client {
public: 
	Client(boost::asio::io_service& io,
		const boost::shared_ptr<ndn::Face>& face,
		const boost::shared_ptr<ndn::KeyChain>& keyChain):io_(io),
		face_(face), keyChain_(keyChain){}
	~Client(){}

	// blocking call. will return after runTimeSec seconds
	void run(unsigned int runTimeSec, unsigned int statSamplePeriodMs, 
		const ClientParams& params);

private:
	boost::asio::io_service& io_;
	unsigned int runTimeSec_, statSampleIntervalMs_;
	ClientParams params_;

	boost::shared_ptr<StatCollector> statCollector_;
	boost::shared_ptr<ndn::Face> face_;
	boost::shared_ptr<ndn::KeyChain> keyChain_;

	std::vector<RemoteStream> remoteStreams_;
	std::vector<LocalStream> localStreams_;

	Client(Client const&) = delete;
	void operator=(Client const&) = delete;

	void setupConsumer();
	void setupProducer();
	void setupStatGathering();
	void runProcessLoop();
	void tearDownStatGathering();
	void tearDownProducer();
	void tearDownConsumer();

	RemoteStream initRemoteStream(const ConsumerStreamParams& p, 
		const ndnrtc::GeneralConsumerParams& generalParams);
	LocalStream initLocalStream(const ProducerStreamParams& p);
	boost::shared_ptr<RawFrame> sampleFrameForStream(const ProducerStreamParams& p);
};

#endif
