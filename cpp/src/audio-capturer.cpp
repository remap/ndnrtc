//
//  audio-capturer.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "audio-capturer.h"
#include "ndnrtc-utils.h"
#include "audio-controller.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost;

//******************************************************************************
#pragma mark - construction/destruction
AudioCapturer::AudioCapturer():
capturing_(false),
rtpDataLen_(0), rtcpDataLen_(0),
isDeliveryScheduled_(false)
{
    description_ = "audio-capturer";
}

AudioCapturer::~AudioCapturer()
{
    AudioController::getSharedInstance()->performOnAudioThread([this](){
        if (isDeliveryScheduled_)
        {
            boost::mutex m;
            boost::unique_lock<boost::mutex> lock(m);
            
            isFrameDelivered_.wait(lock);
        }});
    
    if (rtpData_)
        free(rtpData_);
    if (rtcpData_)
        free(rtcpData_);
}

//******************************************************************************
#pragma mark - public
int AudioCapturer::startCapture()
{
    int res = RESULT_OK;
    
    init();
    
    AudioController::getSharedInstance()->performOnAudioThread([this, &res](){
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
    AudioController::getSharedInstance()->performOnAudioThread([this](){
        capturing_ = false;
        voeBase_->StopSend(webrtcChannelId_);
        voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
        webrtcChannelId_ = -1;
    });
    
    release();
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - private
int AudioCapturer::SendPacket(int channel, const void *data, size_t len)
{
    isDeliveryScheduled_ = true;
    
    if (len > rtpDataLen_)
    {
        rtpData_ = realloc(rtpData_, len);
        rtpDataLen_ = len;
    }
    memcpy(rtpData_, data, rtpDataLen_);
    
    NdnRtcUtils::dispatchOnBackgroundThread([this](){
        if (capturing_ && frameConsumer_)
            frameConsumer_->onDeliverRtpFrame(rtpDataLen_,
                                              (unsigned char*)rtpData_);
    },
                                            [this](){
                                                isDeliveryScheduled_ =  false;
                                                isFrameDelivered_.notify_one();
                                            });
    return len;
}

int AudioCapturer::SendRTCPPacket(int channel, const void *data, size_t len)
{
    isDeliveryScheduled_ = true;
    
    if (len > rtcpDataLen_)
    {
        rtcpData_ = realloc(rtcpData_, len);
        rtcpDataLen_ = len;
    }
    memcpy(rtcpData_, data, rtcpDataLen_);
    
    NdnRtcUtils::dispatchOnBackgroundThread([this](){
        if (capturing_ && frameConsumer_)
            frameConsumer_->onDeliverRtcpFrame(rtcpDataLen_,
                                               (unsigned char*)rtcpData_);
    },
                                            [this](){
                                                isDeliveryScheduled_ =  false;
                                                isFrameDelivered_.notify_one();
                                            });
    return len;
}
