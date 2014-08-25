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

BaseCapturer::BaseCapturer(const ParamsStruct& params):
NdnRtcObject(params),
capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
captureEvent_(*EventWrapper::Create()),
captureThread_(*ThreadWrapper::CreateThread(processCapturedFrame, this,  kHighPriority))
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
    unsigned int tid = 0;
    
    if (!captureThread_.Start(tid))
        return notifyError(RESULT_ERR, "can't start capturing thread");

    return RESULT_OK;
}

int BaseCapturer::stopCapture()
{
    isCapturing_ = false;
    
    captureThread_.SetNotAlive();
    captureEvent_.Set();
    
    if (!captureThread_.Stop())
        return notifyError(-1, "can't stop capturing thread");
    
    return RESULT_OK;
}

bool BaseCapturer::process()
{
    if (captureEvent_.Wait(100) == kEventSignaled)
    {
        NdnRtcUtils::frequencyMeterTick(meterId_);
        
        deliver_cs_->Enter();
        if (!capturedFrame_.IsZeroSize())
        {
            capture_cs_->Enter();
            double timestamp = NdnRtcUtils::unixTimestamp();
            deliverFrame_.SwapFrame(&capturedFrame_);
            capturedFrame_.ResetSize();
            capture_cs_->Leave();
            
            if (frameConsumer_)
                frameConsumer_->onDeliverFrame(deliverFrame_, timestamp);
        }
        deliver_cs_->Leave();
    }

    return true;
}

void BaseCapturer::deliverCapturedFrame(webrtc::I420VideoFrame& frame)
{
    capture_cs_->Enter();
    capturedFrame_.SwapFrame(&frame);
    capture_cs_->Leave();
    
    captureEvent_.Set();
}
