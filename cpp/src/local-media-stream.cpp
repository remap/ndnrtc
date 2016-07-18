// 
// local-media-stream.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/asio.hpp>

#include "local-media-stream.h"
#include "audio-stream-impl.h"
#include "video-stream-impl.h"
#include "audio-capturer.h"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

//******************************************************************************
std::vector<std::pair<std::string, std::string>> 
LocalAudioStream::getRecordingDevices()
{
	return AudioCapturer::getRecordingDevices();
}

std::vector<std::pair<std::string, std::string>> 
LocalAudioStream::getPlayoutDevices()
{
	return AudioCapturer::getPlayoutDevices();
}

//******************************************************************************
LocalAudioStream::LocalAudioStream(const std::string& basePrefix, 
			const MediaStreamSettings& settings):
pimpl_(boost::make_shared<AudioStreamImpl>(basePrefix, settings))
{
	pimpl_->setupInvocation(MediaStreamBase::MetaCheckIntervalMs, 
		boost::bind(&MediaStreamBase::periodicInvocation, pimpl_));
	pimpl_->publishMeta();
}

LocalAudioStream::~LocalAudioStream()
{
	pimpl_->cancelInvocation();
	pimpl_->stop();
}

void LocalAudioStream::start()
{
	pimpl_->start();
}

void LocalAudioStream::stop()
{
	pimpl_->stop();
}

void
LocalAudioStream::addThread(const AudioThreadParams params)
{
	pimpl_->addThread(&params);
}

void LocalAudioStream::removeThread(const string& threadName)
{
	pimpl_->removeThread(threadName);
}

bool LocalAudioStream::isRunning() const 
{
	return pimpl_->isRunning();
}

string
LocalAudioStream::getPrefix() const
{
	return pimpl_->getPrefix();
}

vector<string> 
LocalAudioStream::getThreads() const
{
	return pimpl_->getThreads();
}

void
LocalAudioStream::setLogger(ndnlog::new_api::Logger* logger)
{
	pimpl_->setLogger(logger);
}

statistics::StatisticsStorage 
LocalAudioStream::getStatistics() const
{
	return pimpl_->getStatistics();
}

//******************************************************************************
LocalVideoStream::LocalVideoStream(const std::string& streamPrefix,
	const MediaStreamSettings& settings, bool useFec):
pimpl_(boost::make_shared<VideoStreamImpl>(streamPrefix, settings, useFec))
{
	pimpl_->setupInvocation(MediaStreamBase::MetaCheckIntervalMs, 
		boost::bind(&MediaStreamBase::periodicInvocation, pimpl_));
	pimpl_->publishMeta();
}

LocalVideoStream::~LocalVideoStream()
{
}

void
LocalVideoStream::addThread(const VideoThreadParams params)
{
	pimpl_->addThread(&params);
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

statistics::StatisticsStorage 
LocalVideoStream::getStatistics() const
{
	return pimpl_->getStatistics();
}
