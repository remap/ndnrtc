// 
// local-stream.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/asio.hpp>

#include "local-stream.hpp"
#include "audio-stream-impl.hpp"
#include "video-stream-impl.hpp"
#include "audio-capturer.hpp"

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
	pimpl_->publishMeta(1);
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

std::string 
LocalAudioStream::getBasePrefix() const 
{ 
	return pimpl_->getBasePrefix(); 
}

std::string 
LocalAudioStream::getStreamName() const 
{ 
	return pimpl_->getStreamName(); 
}

vector<string> 
LocalAudioStream::getThreads() const
{
	return pimpl_->getThreads();
}

void
LocalAudioStream::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
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
	pimpl_->publishMeta(1);
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
	unsigned int frameSize,
	unsigned int userDataSize,
	unsigned char* userData)
{
	pimpl_->incomingFrame(ArgbRawFrameWrapper({width, height, argbFrameData, frameSize}),
				userDataSize, userData);
	return RESULT_OK;
}

int LocalVideoStream::incomingI420Frame(const unsigned int width,
	const unsigned int height,
	const unsigned int strideY,
	const unsigned int strideU,
	const unsigned int strideV,
	const unsigned char* yBuffer,
	const unsigned char* uBuffer,
	const unsigned char* vBuffer,
	unsigned int userDataSize,
	unsigned char* userData)
{
	pimpl_->incomingFrame(I420RawFrameWrapper({width, height, strideY, strideU,
		strideV, yBuffer, uBuffer, vBuffer}), userDataSize, userData);
	return RESULT_OK;
}

string
LocalVideoStream::getPrefix() const
{
	return pimpl_->getPrefix();
}

std::string 
LocalVideoStream::getBasePrefix() const 
{ 
	return pimpl_->getBasePrefix(); 
}

std::string 
LocalVideoStream::getStreamName() const 
{ 
	return pimpl_->getStreamName(); 
}

vector<string> 
LocalVideoStream::getThreads() const
{
	return pimpl_->getThreads();
}

void
LocalVideoStream::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
	pimpl_->setLogger(logger);
}

statistics::StatisticsStorage 
LocalVideoStream::getStatistics() const
{
	return pimpl_->getStatistics();
}
