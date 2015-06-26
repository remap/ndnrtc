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
lastFrameTimestamp_(0)
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
    captureThread_ = startThread([this]()->bool{
        return process();
    });

    return RESULT_OK;
}

int BaseCapturer::stopCapture()
{
    isCapturing_ = false;
    deliverEvent_.notify_one();
    stopThread(captureThread_);
    
    return RESULT_OK;
}

bool BaseCapturer::process()
{
    LogTraceC << "check captured frame" << std::endl;
    unique_lock<mutex> captureLock(deliverMutex_);
    
    if (deliverEvent_.wait_for(deliverMutex_, chrono::milliseconds(100)) == cv_status::no_timeout)
    {
        if (!capturedFrame_.IsZeroSize())
        {
            NdnRtcUtils::frequencyMeterTick(meterId_);
        }
        
        if (!capturedFrame_.IsZeroSize())
        {
            LogTraceC << "swapping frames" << std::endl;
            captureMutex_.lock();
            int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
            deliverFrame_.CopyFrame(capturedFrame_);
            //deliverFrame_.SwapFrame(&capturedFrame_);
            //capturedFrame_.ResetSize();
            captureMutex_.unlock();
            LogTraceC << "checking frame consumer" << std::endl;
            
            if (frameConsumer_)
            {
                if (lastFrameTimestamp_)
                {
                    int64_t delay = timestamp - lastFrameTimestamp_;

                    ((delay > FRAME_DELAY_DEADLINE) ? LogWarnC : LogTraceC)
                    << "captured frame delivery delay " << delay << std::endl;
                }
                
                lastFrameTimestamp_ = timestamp;
                frameConsumer_->onDeliverFrame(deliverFrame_, timestamp);
                LogTraceC << "frame consumed" << std::endl;
            }
        }
    }

    return isCapturing_;
}

void BaseCapturer::deliverCapturedFrame(WebRtcVideoFrame& frame)
{
    if (isCapturing_)
    {
        {
            lock_guard<mutex> scopedLock(captureMutex_);
            capturedFrame_.CopyFrame(frame);
        }
        deliverEvent_.notify_one();
    }
}
