// 
// video-source.h
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __video_source_h__
#define __video_source_h__

#include <stdlib.h>
#include <ndnrtc/interfaces.hpp>

#include "config.hpp"
#include "frame-io.hpp"
#include "precise-generator.hpp"

class VideoSource {
public:
	/**
	 * Creates an instance of VideoSource class.
	 * @param io_service Boost IO service to schedule timer on
	 * @param rate Framerate (per second) for delivering frames (reading 
	 * frames from a source file)
	 */
	VideoSource(boost::asio::io_service& io_service, const std::string& sourcePath,
		const boost::shared_ptr<RawFrame>& frame);
	~VideoSource();

	/**
	 * Add ndnrtc external capturer object into which read frames will be
	 * delivered.
	 * Capturers can be added after video source has started.
	 * Thread-safe unless called from different thread than io_.run() is called.
	 */
	void addCapturer(ndnrtc::IExternalCapturer* capturer);

	void start(const double& rate);
	void stop();
	unsigned int getSourcedFramesNumber() { return framesSourced_; }
	unsigned int getRewinds() { return nRewinds_; }
	bool isRunning(){ return isRunning_; }
	double getMeanSourcingTimeMs();

private:
	bool isRunning_;
	unsigned int framesSourced_, nRewinds_, userDataSize_;
	unsigned char* userData_;
	boost::shared_ptr<FileFrameSource> source_;
	boost::shared_ptr<RawFrame> frame_;
	std::vector<ndnrtc::IExternalCapturer*> capturers_;
	boost::asio::io_service& io_;
	boost::shared_ptr<PreciseGenerator> generator_;

	void sourceFrame();
	void deliverFrame(const RawFrame& frame);
};

#endif
