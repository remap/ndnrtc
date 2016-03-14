// 
// client.cpp
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/make_shared.hpp>

#include "client.h"
#include "config.h"
#include "renderer.h"

using namespace std;
using namespace ndnrtc::new_api;
using namespace ndnrtc;

//******************************************************************************
Client::Client():ndnp_(nullptr)
{
}

Client::~Client()
{
}

Client& Client::getSharedInstance()
{
	static Client client;
	return client;
}

void Client::run(ndnrtc::INdnRtcLibrary* ndnp, unsigned int runTimeSec,
	unsigned int statSamplePeriodMs, const ClientParams& params)
{
	ndnp_ = ndnp;
	ndnp_->setObserver(&libObserver_);
	runTimeSec_ = runTimeSec;
	statSampleIntervalMs_ = statSamplePeriodMs;
	params_ = params;

	setupConsumer();
	setupProducer();
	setupStatGathering();

	runProcessLoop();

	tearDownStatGathering();
	tearDownProducer();
	tearDownConsumer();
}

//******************************************************************************
void Client::setupConsumer()
{
	if (!params_.isConsuming())
		return;

	ConsumerClientParams ccp = params_.getConsumerParams();

	for (auto p:ccp.fetchedStreams_)
	{
		GeneralConsumerParams gp = (p.type_ == ClientMediaStreamParams::MediaStreamType::MediaStreamTypeAudio ? ccp.generalAudioParams_ : ccp.generalVideoParams_);
		RemoteStream rs = initRemoteStream(p, gp);
#warning check move semantics
		remoteStreams_.push_back(boost::move(rs));
	}

	LogInfo("") << "Set up fetching " << remoteStreams_.size() << " remote stream(s)" << endl;
}

void Client::setupProducer()
{
	if (!params_.isProducing())
		return;

	initSession();
}

void Client::setupStatGathering()
{
	if (!params_.isGatheringStats())
		return;

	LogInfo("") << "new stat colllector" << endl;
	statCollector_.reset(new StatCollector(io_, ndnp_));
	
	for (auto& rs:remoteStreams_)
		statCollector_->addStream(rs.getPrefix());

	statCollector_->startCollecting(statSampleIntervalMs_, 
		params_.getGeneralParameters().logPath_, 
		params_.getConsumerParams().statGatheringParams_);

	LogInfo("") << "Gathering statistics into " 
		<< statCollector_->getWritersNumber() << " files" << std::endl;
}

void Client::runProcessLoop()
{
    boost::shared_ptr<boost::asio::io_service::work> work;
    boost::asio::deadline_timer runTimer(io_);

    runTimer.expires_from_now(boost::posix_time::seconds(runTimeSec_));
    runTimer.async_wait([&](const boost::system::error_code& ){
        work.reset();
        io_.stop();
    });

    LogDebug("") << "ndnrtc client started..." << std::endl;
    
    work.reset(new boost::asio::io_service::work(io_));
    io_.run();
    io_.reset();

    LogDebug("") << "ndnrtc client completed" << std::endl;
}

void Client::tearDownStatGathering(){
	if (!params_.isGatheringStats())
		return;

	statCollector_->stop();
}

void Client::tearDownProducer(){
	if (!params_.isProducing())
		return;

	LogInfo("Tearing down producing...");

	ndnp_->stopSession(clientSessionObserver_.getSessionPrefix());
	LogInfo("") << "...stopped session" << std::endl;
}

void Client::tearDownConsumer(){
	if (!params_.isConsuming())
		return ;

	LogInfo("") << "Tearing down consuming..." << std::endl;

	for (auto& rs:remoteStreams_)
	{
		ndnp_->removeRemoteStream(rs.getPrefix());
		LogInfo("") << "...stopped fetching from " << rs.getPrefix() << std::endl;
	}
	remoteStreams_.clear();
}

//******************************************************************************
void Client::initSession()
{
	string username = params_.getProducerParams().username_;
	string prefix = params_.getProducerParams().prefix_;
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

RemoteStream Client::initRemoteStream(const ConsumerStreamParams& p,
	const GeneralConsumerParams& gcp)
{
	RendererInternal *renderer = (p.type_ == ConsumerStreamParams::MediaStreamTypeVideo ? new RendererInternal() : nullptr);
	string streamPrefix = ndnp_->addRemoteStream(p.sessionPrefix_, p.threadToFetch_, p, 
		params_.getGeneralParameters(), gcp, renderer);

	return RemoteStream(streamPrefix, boost::shared_ptr<RendererInternal>(renderer));
}