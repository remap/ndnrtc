// 
// video-source.cpp
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <sstream>
#include "video-source.hpp"

using namespace std;
using namespace boost::chrono;
using namespace boost::asio;

VideoSource::VideoSource(io_service& io_service, 
	const std::string& sourcePath, 
	const boost::shared_ptr<RawFrame>& frame):
io_(io_service), frame_(frame), isRunning_(false), framesSourced_(0),
nRewinds_(0), userDataSize_(0), userData_(nullptr)
{
	assert(frame.get());

	if (!FileFrameSource::checkSourceForFrame(sourcePath, *frame_))
	{
		stringstream msg;
		msg << "bad source (" << sourcePath << ") for " 
			<< frame_->getWidth() << "x" << frame_->getHeight() << " video";
		throw runtime_error(msg.str());
	}

	source_.reset(new FileFrameSource(sourcePath));
}

VideoSource::~VideoSource()
{
    stop();
}

void VideoSource::addCapturer(ndnrtc::IExternalCapturer* capturer)
{
	capturers_.push_back(capturer);
}

void VideoSource::start(const double& rate)
{
	assert(rate);

	if (!isRunning_)
	{
		framesSourced_ = 0;

		generator_.reset(new PreciseGenerator(io_, rate, [this](){
			this->sourceFrame();
			this->deliverFrame(*frame_);
		}));
		generator_->start();

		isRunning_ = generator_->isRunning();

		LogInfo("") << "sourcing video from " << source_->getPath() 
			<< " started (capturing rate " << rate << ")" << endl;
	}
	else
		throw runtime_error("video source has been already started");
}

void VideoSource::stop()
{
	if (isRunning_)
	{
		generator_->stop();
		isRunning_ = false;

		LogInfo("") << "sourcing video from " << source_->getPath() 
			<< " stopped" << endl;
	}
}

double VideoSource::getMeanSourcingTimeMs()
{
	if (generator_.get())
		return generator_->getMeanTaskTimeNs()/1000000.;
	return 0.;
}

#pragma mark - private
void VideoSource::sourceFrame()
{
	// LogTrace("") << "reading " << frame_->getWidth() << "x" << frame_->getHeight() 
		// << "frame from " << source_->getPath() << endl;

	do{
		if (source_->isEof())
		{
			LogDebug("") << "rewound source " << source_->getPath() << endl;
			source_->rewind();
			nRewinds_++;
		}
		
		*source_ >> *frame_;
	} while(source_->isEof());
	
	framesSourced_++;
}

void VideoSource::deliverFrame(const RawFrame& frame)
{
	for (auto capturer:capturers_)
		capturer->incomingArgbFrame(frame.getWidth(), frame.getHeight(),
			frame.getBuffer().get(), frame.getFrameSizeInBytes(),
			userDataSize_, userData_);

	// LogTrace("") << "delivered frame to " << capturers_.size() << " capturers" << endl;
}



