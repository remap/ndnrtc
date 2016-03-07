// 
// client.cpp
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "client.h"
#include "config.h"
#include "renderer.h"

using namespace std;
using namespace ndnrtc::new_api;
using namespace ndnrtc;

//******************************************************************************
Client::Client()
{
	ndnp_ = &(NdnRtcLibrary::getSharedInstance());
}

Client::~Client()
{
}

Client& Client::getSharedInstance()
{
	static Client client;
	return client;
}

void Client::setLibraryObserver(INdnRtcLibraryObserver& ndnrtcLibObserver)
{
	ndnp_->setObserver(&ndnrtcLibObserver);
}

void Client::run(unsigned int runTimeSec, unsigned int statSamplePeriodMs, 
	const ClientParams& params)
{
	runTimeSec_ = runTimeSec;
	statSampleIntervalMs_ = statSamplePeriodMs;
	params_ = params;

	initSession();
	setupConsumer();
	setupProducer();
	setupStatGathering();
	
	runProcessLoop();

	tearDownStatGathering();
	tearDownProducer();
	tearDownConsumer();

	ndnp_->stopSession(clientSessionObserver_.getSessionPrefix());
}

//******************************************************************************
void Client::initSession()
{
	string username = (params_.isProducing() ? params_.getProducerParams().username_ : "headless-consumer");
	string prefix = (params_.isProducing() ? params_.getProducerParams().prefix_ : "/incognito");

	string sessionPrefix = ndnp_->startSession(username, prefix, params_.getGeneralParameters(), 
		&clientSessionObserver_);

	if (sessionPrefix != "")
	{
		clientSessionObserver_.setSessionPrefix(sessionPrefix);
		clientSessionObserver_.setPrefix(prefix);
		clientSessionObserver_.setUsername(username);

		LogInfo("") << "initialized client session with username " 
		<< username
		<< " prefix " << prefix
		<< " (session prefix " << sessionPrefix << ")" << std::endl;
	}
	else
		throw std::runtime_error("couldn't initialize client session");
}

void Client::setupConsumer()
{
	if (!params_.isConsuming())
		return;

	ConsumerClientParams ccp = params_.getConsumerParams();

	for (auto p:ccp.fetchedStreams_)
	{
		GeneralConsumerParams gp = (p.type_ == ClientMediaStreamParams::MediaStreamType::MediaStreamTypeAudio ? ccp.generalAudioParams_ : ccp.generalVideoParams_);
		remoteStreams_.push_back(initRemoteStream(p, gp));
	}
}

void Client::setupProducer()
{

}

void Client::setupStatGathering()
{

}

void Client::runProcessLoop()
{
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work;
    boost::asio::deadline_timer runTimer(io);

    runTimer.expires_from_now(boost::posix_time::seconds(runTimeSec_));
    runTimer.async_wait([&](const boost::system::error_code& ){
        work.reset();
    });

    LogDebug("") << "ndnrtc client started..." << std::endl;
    
    work.reset(new boost::asio::io_service::work(io));
    io.run();

    LogDebug("") << "ndnrtc client completed" << std::endl;
}

void Client::tearDownStatGathering(){

}

void Client::tearDownProducer(){

}

void Client::tearDownConsumer(){
	for (auto rs:remoteStreams_)
		ndnp_->removeRemoteStream(rs.getPrefix());
}


//******************************************************************************
RemoteStream Client::initRemoteStream(const ConsumerStreamParams& p,
	const GeneralConsumerParams& gcp)
{
	string streamPrefix = ndnp_->addRemoteStream(p.sessionPrefix_, p.threadToFetch_, p, 
		params_.getGeneralParameters(), gcp, new RendererInternal());

	return RemoteStream(streamPrefix);
}