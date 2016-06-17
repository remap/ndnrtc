// 
// remote-stream.cpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-stream.h"
#include <boost/make_shared.hpp>

#include "remote-stream-impl.h"

using namespace ndnrtc;

//******************************************************************************
RemoteStream::RemoteStream(boost::asio::io_service& faceIo, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
pimpl_(boost::make_shared<RemoteStreamImpl>(faceIo, face, keyChain, streamPrefix))
{

}

RemoteStream::~RemoteStream(){}

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
RemoteStream::start(const std::string& threadName)
{
	pimpl_->start(threadName);
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

void
RemoteStream::attach(IRemoteStreamObserver* o)
{
	pimpl_->attach(o);
}

void 
RemoteStream::detach(IRemoteStreamObserver* o)
{
	pimpl_->detach(o);
}
