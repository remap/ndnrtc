//
//  audio-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "audio-capturer.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

//******************************************************************************
#pragma mark - construction/destruction
AudioCapturer::AudioCapturer()
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
    if (voeNetwork_->RegisterExternalTransport(webrtcChannelId_, *this) < 0)
        return notifyError(RESULT_ERR, "can't register external transport for "
                           "WebRTC due to error (code %d)",
                           voeBase_->LastError());
    
    if (voeBase_->StartSend(webrtcChannelId_) < 0)
        return notifyError(RESULT_ERR, "can't start send channel due to "
                           "WebRTC error (code %d)", voeBase_->LastError());
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int AudioCapturer::stopCapture()
{
    voeBase_->StopSend(webrtcChannelId_);
    voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
    webrtcChannelId_ = -1;
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - private
int AudioCapturer::SendPacket(int channel, const void *data, size_t len)
{
    if (frameConsumer_)
        frameConsumer_->onDeliverRtpFrame(len, (unsigned char*)data);
    
    return len;
}

int AudioCapturer::SendRTCPPacket(int channel, const void *data, size_t len)
{
    if (frameConsumer_)
        frameConsumer_->onDeliverRtcpFrame(len, (unsigned char*)data);
    
    return len;
}
