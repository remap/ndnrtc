//
//  capturer.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <boost/thread/lock_guard.hpp>
#include <boost/chrono.hpp>

#include "base-capturer.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace boost;

BaseCapturer::BaseCapturer():
lastFrameTimestamp_(0),
meterId_(NdnRtcUtils::setupFrequencyMeter(4))
{
    description_ = "capturer";
}

BaseCapturer::~BaseCapturer()
{
    
}

int BaseCapturer::init()
{
    return RESULT_OK;
}

int BaseCapturer::startCapture()
{
    return RESULT_OK;
}

int BaseCapturer::stopCapture()
{
    isCapturing_ = false;
    stopJob();
    
    return RESULT_OK;
}

bool BaseCapturer::process()
{
    LogTraceC << "check captured frame" << std::endl;
    if (!capturedFrame_.IsZeroSize())
    {
        NdnRtcUtils::frequencyMeterTick(meterId_);
        double timestamp = NdnRtcUtils::unixTimestamp();
    
        if (frameConsumer_)
        {
            if (lastFrameTimestamp_)
            {
                int64_t delay = timestamp - lastFrameTimestamp_;
    
                ((delay > FRAME_DELAY_DEADLINE) ? LogWarnC : LogTraceC)
                << "captured frame delivery delay " << delay << std::endl;
            }
            
            lastFrameTimestamp_ = timestamp;
            frameConsumer_->onDeliverFrame(capturedFrame_, timestamp);
            
            LogTraceC << "frame consumed" << std::endl;
        }
    }

    return false;
}

void BaseCapturer::deliverCapturedFrame(WebRtcVideoFrame& frame)
{
    if (isCapturing_)
    {
        lock_guard<recursive_mutex> scopedLock(jobMutex_);
        capturedFrame_.CopyFrame(frame);
    }
}
