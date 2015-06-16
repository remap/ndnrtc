//
//  capturer.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "base-capturer.h"

using namespace ndnrtc;
using namespace webrtc;

BaseCapturer::BaseCapturer():
capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
captureEvent_(*EventWrapper::Create()),
captureThread_(*ThreadWrapper::CreateThread(processCapturedFrame, this, "capture-thread")),
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
    if (!captureThread_.Start())
        return notifyError(RESULT_ERR, "can't start capturing thread");

    return RESULT_OK;
}

int BaseCapturer::stopCapture()
{
    isCapturing_ = false;
    
//    captureThread_.SetNotAlive();
    captureEvent_.Set();
    
    if (!captureThread_.Stop())
        return notifyError(-1, "can't stop capturing thread");
    
    return RESULT_OK;
}

bool BaseCapturer::process()
{
    LogTraceC << "check captured frame" << std::endl;
    if (captureEvent_.Wait(100) == kEventSignaled)
    {
        if (!capturedFrame_.IsZeroSize())
        {
            NdnRtcUtils::frequencyMeterTick(meterId_);
        }
        
        deliver_cs_->Enter();
        if (!capturedFrame_.IsZeroSize())
        {
            LogTraceC << "swapping frames" << std::endl;
            capture_cs_->Enter();
            int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
            deliverFrame_.CopyFrame(capturedFrame_);
            //deliverFrame_.SwapFrame(&capturedFrame_);
            //capturedFrame_.ResetSize();
            capture_cs_->Leave();
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
        deliver_cs_->Leave();
    }

    return true;
}

void BaseCapturer::deliverCapturedFrame(WebRtcVideoFrame& frame)
{
    if (isCapturing_)
    {
        capture_cs_->Enter();
        capturedFrame_.CopyFrame(frame);
//        capturedFrame_.SwapFrame(&frame);
        capture_cs_->Leave();
        
        captureEvent_.Set();
    }
}
