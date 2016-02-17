//
//  audio-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "audio-capturer.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

//******************************************************************************
#pragma mark - construction/destruction
AudioCapturer::AudioCapturer():
capturing_(false)
{
    description_ = "audio-capturer";
}

AudioCapturer::~AudioCapturer()
{
}

//******************************************************************************
#pragma mark - public
int AudioCapturer::startCapture()
{
    int res = RESULT_OK;
    
    init();
    
    NdnRtcUtils::performOnBackgroundThread([this, &res](){
        if (voeNetwork_->RegisterExternalTransport(webrtcChannelId_, *this) < 0)
            res = notifyError(RESULT_ERR, "can't register external transport for "
                               "WebRTC due to error (code %d)",
                               voeBase_->LastError());
        
        if (voeBase_->StartSend(webrtcChannelId_) < 0)
            res = notifyError(RESULT_ERR, "can't start send channel due to "
                               "WebRTC error (code %d)", voeBase_->LastError());
    });
    
    if (RESULT_GOOD(res))
    {
        capturing_ = true;
        LogInfoC << "started" << endl;
    }
    
    return res;
}

int AudioCapturer::stopCapture()
{
    capturing_ = false;
    release();
    
    NdnRtcUtils::performOnBackgroundThread([this](){
        voeBase_->StopSend(webrtcChannelId_);
        voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
        webrtcChannelId_ = -1;
    });
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - private
int AudioCapturer::SendPacket(int channel, const void *data, size_t len)
{
    if (capturing_ && frameConsumer_)
        frameConsumer_->onDeliverRtpFrame(len, (unsigned char*)data);
    
    return len;
}

int AudioCapturer::SendRTCPPacket(int channel, const void *data, size_t len)
{
    if (capturing_ && frameConsumer_)
        frameConsumer_->onDeliverRtcpFrame(len, (unsigned char*)data);
    
    return len;
}
