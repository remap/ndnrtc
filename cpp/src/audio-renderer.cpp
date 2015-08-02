//
//  audio-renderer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "audio-renderer.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
AudioRenderer::AudioRenderer():
WebrtcAudioChannel()
{
    description_ = "arenderer";
}

AudioRenderer::~AudioRenderer()
{
}

//******************************************************************************
#pragma mark - public
int
AudioRenderer::init()
{
    LogInfoC << "initialized" << endl;
    return RESULT_OK;
}

int
AudioRenderer::startRendering(const std::string &name)
{
    if (RESULT_FAIL(WebrtcAudioChannel::init()))
    {
        LogErrorC << "can't instantiate WebRTC voice channel \
        due to error (code: " << voeBase_->LastError() << ")" << endl;
        return RESULT_ERR;
    }
    
    NdnRtcUtils::performOnBackgroundThread([this](){
                                           // register external transport in order to playback. however, we are not
                                           // going to set this channel for sending and should not be getting callback
                                           // on webrtc::Transport callbacks
                                           if (voeNetwork_->RegisterExternalTransport(webrtcChannelId_, *this) < 0)
                                               notifyError(RESULT_ERR, "can't register external transport for "
                                                           "WebRTC due to error (code %d)",
                                                           voeBase_->LastError());
                                           else if (voeBase_->StartReceive(webrtcChannelId_) < 0)
                                               notifyError(RESULT_ERR, "can't start receiving channel due to "
                                                           "WebRTC error (code %d)", voeBase_->LastError());
                                           
                                           else if (voeBase_->StartPlayout(webrtcChannelId_) < 0)
                                               notifyError(RESULT_ERR, "can't start playout audio due to WebRTC "
                                                           "error (code %d)", voeBase_->LastError());
                                           else
                                               isRendering_ = true;
                                           LogInfoC << "started" << endl;
                                       });
    
    return (isRendering_)?RESULT_OK:RESULT_ERR;
}

int
AudioRenderer::stopRendering()
{
    NdnRtcUtils::performOnBackgroundThread(
                                       [this](){
                                           if (isRendering_)
                                           {
                                               voeBase_->StopPlayout(webrtcChannelId_);
                                               voeBase_->StopReceive(webrtcChannelId_);
                                               voeNetwork_->DeRegisterExternalTransport(webrtcChannelId_);
                                           }
                                           webrtcChannelId_ = -1;
                                           isRendering_ = false;
                                       });
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

void
AudioRenderer::onDeliverRtpFrame(unsigned int len, unsigned char *data)
{
    if (voeNetwork_->ReceivedRTPPacket(webrtcChannelId_,
                                       data,
                                       len) < 0)
        notifyError(RESULT_WARN, "can't playback audio packet (RTP) due to WebRTC error "
                    "(code %d)", voeBase_->LastError());
}

void
AudioRenderer::onDeliverRtcpFrame(unsigned int len, unsigned char *data)
{
    if (voeNetwork_->ReceivedRTCPPacket(webrtcChannelId_,
                                        data,
                                        len) < 0)
        notifyError(RESULT_WARN, "can't playback audio packet (RTCP) due to WebRTC error "
                    "(code %d)", voeBase_->LastError());
    
}