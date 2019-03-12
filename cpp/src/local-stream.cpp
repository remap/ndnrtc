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

boost::shared_ptr<ndnlog::new_api::Logger> 
LocalAudioStream::getLogger() const
{
    return pimpl_->getLogger();
}


statistics::StatisticsStorage 
LocalAudioStream::getStatistics() const
{
	return pimpl_->getStatistics();
}

boost::shared_ptr<StorageEngine> 
LocalAudioStream::getStorage() const
{
    return pimpl_->getStorage();
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
	return pimpl_->incomingFrame(ArgbRawFrameWrapper({width, height, argbFrameData, frameSize, true}));
}

int LocalVideoStream::incomingBgraFrame(const unsigned int width,
	const unsigned int height,
	unsigned char* bgraFrameData,
	unsigned int frameSize)
{
	return pimpl_->incomingFrame(BgraRawFrameWrapper({width, height, bgraFrameData, frameSize, false}));
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
	return pimpl_->incomingFrame(I420RawFrameWrapper({width, height, strideY, strideU,
		strideV, yBuffer, uBuffer, vBuffer}));
}

int LocalVideoStream::incomingNV21Frame(const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideUV,
			const unsigned char* yBuffer,
			const unsigned char* uvBuffer)
{
	return pimpl_->incomingFrame(YUV_NV21FrameWrapper({width, height, strideY, 
		strideUV, yBuffer, uvBuffer}));
}

const std::map<std::string, FrameInfo>& 
LocalVideoStream::getLastPublishedInfo() const
{
    return pimpl_->getLastPublished();
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

boost::shared_ptr<ndnlog::new_api::Logger> 
LocalVideoStream::getLogger() const
{
    return pimpl_->getLogger();
}

statistics::StatisticsStorage 
LocalVideoStream::getStatistics() const
{
	return pimpl_->getStatistics();
}

boost::shared_ptr<StorageEngine> 
LocalVideoStream::getStorage() const
{
    return pimpl_->getStorage();
}