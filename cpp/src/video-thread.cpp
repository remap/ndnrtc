//
//  video-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//
#include "video-thread.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
VideoThread::VideoThread(const VideoCoderParams& coderParams):
coder_(coderParams, this, VideoCoder::KeyEnforcement::Timed),
nEncoded_(0), nDropped_(0)
{
    description_ = "vthread";
}

VideoThread::~VideoThread()
{
}

//******************************************************************************
#pragma mark - public
boost::shared_ptr<VideoFramePacket> 
VideoThread::encode(const WebRtcVideoFrame& frame)
{
    coder_.onRawFrame(frame);
    // result should be delivered using onEncodedFrame or onDroppedFrame
    // callbacks which prepare videoFramePacket_ accordingly
    // here, we just need to return this packet back to a caller
    return boost::move(videoFramePacket_);
}

//******************************************************************************
#pragma mark - interfaces realization
void
VideoThread::onEncodingStarted()
{}

void VideoThread::onEncodedFrame(const webrtc::EncodedImage &encodedImage)
{
    nEncoded_++;
    videoFramePacket_ = boost::make_shared<VideoFramePacket>(encodedImage);
}

void VideoThread::onDroppedFrame()
{
    nDropped_++;
}

void
VideoThread::setLogger(ndnlog::new_api::Logger *logger)
{
    coder_.setLogger(logger);
    ILoggingObject::setLogger(logger);
}
