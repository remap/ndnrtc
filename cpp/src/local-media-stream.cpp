// 
// local-media-stream.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/asio.hpp>

#include "local-media-stream.h"
#include "video-stream-impl.h"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

LocalVideoStream::LocalVideoStream(const std::string& streamPrefix,
	const MediaStreamSettings& settings, bool useFec):
pimpl_(boost::make_shared<VideoStreamImpl>(streamPrefix, settings, useFec))
{
	pimpl_->publishMeta();
	pimpl_->setupMetaCheckTimer();
}

LocalVideoStream::~LocalVideoStream()
{
	pimpl_->metaCheckTimer_.cancel();
}

void
LocalVideoStream::addThread(const VideoThreadParams* params)
{
	pimpl_->addThread(params);
}

void LocalVideoStream::removeThread(const string& threadName)
{
	pimpl_->removeThread(threadName);
}

int LocalVideoStream::incomingArgbFrame(const unsigned int width,
	const unsigned int height,
	unsigned char* argbFrameData,
	unsigned int frameSize)
{
	pimpl_->incomingFrame(ArgbRawFrameWrapper({width, height, argbFrameData, frameSize}));
	return RESULT_OK;
}

int LocalVideoStream::incomingI420Frame(const unsigned int width,
	const unsigned int height,
	const unsigned int strideY,
	const unsigned int strideU,
	const unsigned int strideV,
	const unsigned char* yBuffer,
	const unsigned char* uBuffer,
	const unsigned char* vBuffer)
{
	pimpl_->incomingFrame(I420RawFrameWrapper({width, height, strideY, strideU,
		strideV, yBuffer, uBuffer, vBuffer}));
	return RESULT_OK;
}

string
LocalVideoStream::getPrefix() const
{
	return pimpl_->getPrefix();
}

vector<string> 
LocalVideoStream::getThreads() const
{
	return pimpl_->getThreads();
}

void
LocalVideoStream::setLogger(ndnlog::new_api::Logger* logger)
{
	pimpl_->setLogger(logger);
}