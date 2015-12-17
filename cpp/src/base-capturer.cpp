//
//  capturer.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#define TEST_USE_THREAD

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
#ifdef TEST_USE_THREAD
    captureThread_ = startThread([this]()->bool{
        return process();
    });
#endif

    return RESULT_OK;
}

int BaseCapturer::stopCapture()
{
    isCapturing_ = false;
#ifdef TEST_USE_THREAD
    deliverEvent_.notify_one();
    stopThread(captureThread_);
#else
    stopJob();
#endif
    
    return RESULT_OK;
}

bool BaseCapturer::process()
{
    LogTraceC << "check captured frame" << std::endl;
#ifdef TEST_USE_THREAD
    unique_lock<mutex> captureLock(deliverMutex_);
    if (deliverEvent_.wait_for(deliverMutex_, chrono::milliseconds(100)) == cv_status::no_timeout)
#endif
    {
        if (!capturedFrame_.IsZeroSize())
        {
            NdnRtcUtils::frequencyMeterTick(meterId_);
            double timestamp = NdnRtcUtils::unixTimestamp();
#ifdef TEST_USE_THREAD
            captureMutex_.lock();
            deliverFrame_.CopyFrame(capturedFrame_);
            captureMutex_.unlock();
#endif

            if (frameConsumer_)
            {
                if (lastFrameTimestamp_)
                {
                    int64_t delay = timestamp - lastFrameTimestamp_;

                    ((delay > FRAME_DELAY_DEADLINE) ? LogWarnC : LogTraceC)
                    << "captured frame delivery delay " << delay << std::endl;
                }
                
                lastFrameTimestamp_ = timestamp;
#ifdef TEST_USE_THREAD
                frameConsumer_->onDeliverFrame(deliverFrame_, timestamp);
#else
                frameConsumer_->onDeliverFrame(capturedFrame_, timestamp);
#endif
                LogTraceC << "frame consumed" << std::endl;
            }
        }
    }

#ifdef TEST_USE_THREAD
    return isCapturing_;
#else
    return false;
#endif
}

void BaseCapturer::deliverCapturedFrame(WebRtcVideoFrame& frame)
{
    if (isCapturing_)
    {
        {
#ifdef TEST_USE_THREAD
            lock_guard<mutex> scopedLock(captureMutex_);
#else
            lock_guard<recursive_mutex> scopedLock(jobMutex_);
#endif
            capturedFrame_.CopyFrame(frame);
#ifndef TEST_USE_THREAD
            if (!isJobScheduled_)
                scheduleJob(0, boost::bind(&BaseCapturer::process, this));
#endif
        }
#ifdef TEST_USE_THREAD
        deliverEvent_.notify_one();
#endif
    }
}
