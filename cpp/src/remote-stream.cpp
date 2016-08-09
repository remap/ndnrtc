// 
// remote-stream.cpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-stream.h"
#include <boost/make_shared.hpp>
#include <ndn-cpp/name.hpp>

#include "remote-stream-impl.h"
#include "remote-video-stream.h"
#include "remote-audio-stream.h"

using namespace ndnrtc;

//******************************************************************************
RemoteStream::RemoteStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName):
basePrefix_(basePrefix), streamName_(streamName)
{
}

RemoteStream::~RemoteStream(){
	pimpl_->setNeedsMeta(false);
}

bool
RemoteStream::isMetaFetched() const
{
	return pimpl_->isMetaFetched();
}

std::vector<std::string> 
RemoteStream::getThreads() const
{
	return pimpl_->getThreads();
}

void
RemoteStream::setThread(const std::string& threadName)
{
	pimpl_->setThread(threadName);
}

void 
RemoteStream::stop()
{
	pimpl_->stop();
}

void
RemoteStream::setInterestLifetime(unsigned int lifetime)
{
	pimpl_->setInterestLifetime(lifetime);
}

void
RemoteStream::setTargetBufferSize(unsigned int bufferSize)
{
	pimpl_->setTargetBufferSize(bufferSize);
}

statistics::StatisticsStorage
RemoteStream::getStatistics() const
{
	return pimpl_->getStatistics();
}

void
RemoteStream::setLogger(ndnlog::new_api::Logger* logger)
{
	pimpl_->setLogger(logger);
}

bool
RemoteStream::isVerified() const
{
	return pimpl_->isVerified();
}

//******************************************************************************
RemoteAudioStream::RemoteAudioStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName):
RemoteStream(faceIo, face, keyChain, basePrefix, streamName)
{
	streamPrefix_ = NameComponents::audioStreamPrefix(basePrefix).append(streamName).toUri();
	pimpl_ = boost::make_shared<RemoteAudioStreamImpl>(faceIo, face, keyChain, streamPrefix_);
	pimpl_->fetchMeta();
}

void
RemoteAudioStream::start(const std::string& threadName)
{
	pimpl_->start(threadName);
}

//******************************************************************************
RemoteVideoStream::RemoteVideoStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& basePrefix,
			const std::string& streamName):
RemoteStream(faceIo, face, keyChain, basePrefix, streamName)
{
	streamPrefix_ = NameComponents::videoStreamPrefix(basePrefix).append(streamName).toUri();
	pimpl_ = boost::make_shared<RemoteVideoStreamImpl>(faceIo, face, keyChain, streamPrefix_);
	pimpl_->fetchMeta();
}

void
RemoteVideoStream::start(const std::string& threadName, IExternalRenderer* renderer)
{
	boost::dynamic_pointer_cast<RemoteVideoStreamImpl>(pimpl_)->start(threadName, renderer);
}