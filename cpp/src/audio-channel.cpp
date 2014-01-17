//
//  ndnaudio-transport.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 10/21/13
//

#include "audio-channel.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnAudioChannel::NdnAudioChannel(const ParamsStruct &params, VoiceEngine *voiceEngine):
NdnRtcObject(params)
{
    voe_network_ = VoENetwork::GetInterface(voiceEngine);
    voe_base_ = VoEBase::GetInterface(voiceEngine);
}
NdnAudioChannel::~NdnAudioChannel()
{
    voe_base_->DeleteChannel(channel_);
    voe_base_->Release();
    voe_network_->Release();
}

//******************************************************************************
#pragma mark - public
int NdnAudioChannel::init(shared_ptr<Face> &face)
{
    if (initialized_)
        return notifyError(RESULT_ERR, "audio channel already initialized");
    
    // setup voice channel
    channel_ = voe_base_->CreateChannel();
    
    if (channel_ < 0)
        return notifyError(RESULT_ERR, "can't instantiate WebRTC voice channel \
                           due to error (code: %d)", voe_base_->LastError());
    
    return RESULT_OK;
}

int NdnAudioChannel::start()
{
    if (!initialized_)
        return notifyError(RESULT_ERR, "audio channel was not initialized");
    
    if (started_)
        return notifyError(RESULT_ERR, "audio channel already started");
    
    return RESULT_OK;
}

int NdnAudioChannel::stop()
{
    if (!started_)
        return notifyError(RESULT_ERR, "audio channel was not started");
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - webrtc::Transport callbacks
int NdnAudioChannel::SendPacket(int channel, const void *data, int len)
{
    TRACE("sending RTP is not supposed to be called");
    return len;
}

int NdnAudioChannel::SendRTCPPacket(int channel, const void *data, int len)
{
    TRACE("sending RTCP is not supposed to be called");
    return len;
}

//******************************************************************************
//******************************************************************************
#pragma mark - public
int NdnAudioReceiveChannel::init(shared_ptr<Face> &face)
{
    int res = NdnAudioChannel::init(face);
    
    if (RESULT_FAIL(res))
        return res;
    
    if (!audioReceiver_)
    {
        audioReceiver_ = new NdnAudioReceiver(params_);
        audioReceiver_->setObserver(this);
        audioReceiver_->setFrameConsumer(this);
        audioReceiver_->setLogger(logger_);
    }
    
    if (RESULT_GOOD((res = audioReceiver_->init(face))))
    {
        initialized_ = true;
        DBG("audio receive channel initialized");
    }
    
    return res;
}

int NdnAudioReceiveChannel::start()
{
    int res = NdnAudioChannel::start();
    
    if (RESULT_FAIL(res))
        return res;
    
    // register external transport in order to playback. however, we are not
    // going to set this channel for sending and should not be getting callback
    // on webrtc::Transport callbacks
    if (voe_network_->RegisterExternalTransport(channel_, *this) < 0)
        return notifyError(RESULT_ERR, "can't register external transport for "
                           "WebRTC due to error (code %d)",
                           voe_base_->LastError());
    
    if (voe_base_->StartReceive(channel_) < 0)
        return notifyError(RESULT_ERR, "can't start receiving channel due to "
                           "WebRTC error (code %d)", voe_base_->LastError());
    
    if (voe_base_->StartPlayout(channel_) < 0)
        return notifyError(RESULT_ERR, "can't start playout audio due to WebRTC "
                           "error (code %d)", voe_base_->LastError());
    
    if (audioReceiver_)
        res = audioReceiver_->startFetching();
    
    started_ = RESULT_GOOD(res);
    
    if (started_)
        DBG("audio receive channel started");
    
    return (started_)?RESULT_OK :
    notifyError(RESULT_ERR, "audio receiver was not initialized");
}

int NdnAudioReceiveChannel::stop()
{
    if (RESULT_FAIL(NdnAudioChannel::stop()))
        return RESULT_ERR;
    
    audioReceiver_->stopFetching();
    voe_base_->StopPlayout(channel_);
    voe_base_->StopReceive(channel_);
    voe_network_->DeRegisterExternalTransport(channel_);
    channel_ = -1;
    
    started_ = false;
    DBG("audio receive channel stopped");
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - IAudioPacketConsumer
void NdnAudioReceiveChannel::onRTPPacketReceived(unsigned int len,
                                                 unsigned char *data)
{
    if (voe_network_->ReceivedRTPPacket(channel_, data, len) < 0)
        notifyError(RESULT_WARN, "can't playback audio packet (RTP) due to WebRTC error "
                    "(code %d)", voe_base_->LastError());
}

void NdnAudioReceiveChannel::onRTCPPacketReceived(unsigned int len,
                                                  unsigned char *data)
{
    if (voe_network_->ReceivedRTCPPacket(channel_, data, len) < 0)
        notifyError(RESULT_WARN, "can't playback audio packet (RTCP) due to WebRTC error "
                    "(code %d)", voe_base_->LastError());
}

//******************************************************************************
//******************************************************************************
#pragma mark - public
int NdnAudioSendChannel::init(shared_ptr<ndn::Transport> &transport)
{    
    int res = RESULT_OK;
    shared_ptr<Face> nullFace(nullptr);
    
    if (RESULT_FAIL((res = NdnAudioChannel::init(nullFace))))
        return res;
    
    if (!audioSender_)
    {
        audioSender_ = new NdnAudioSender(params_);
        audioSender_->setObserver(this);
        audioSender_->setLogger(logger_);
    }
    
    if (RESULT_GOOD((res = audioSender_->init(transport))))
        initialized_ = true;
    
    DBG("initialized audio send channel");
    return res;
}

int NdnAudioSendChannel::start()
{
    int res = NdnAudioChannel::start();
    
    if (RESULT_FAIL(res))
        return res;
    
    res = voe_network_->RegisterExternalTransport(channel_, *this);
    
    if (res < 0)
        return notifyError(RESULT_ERR, "can't register external transport for "
                           "WebRTC due to error (code %d)",
                           voe_base_->LastError());
    
    if (voe_base_->StartSend(channel_) < 0)
        return notifyError(RESULT_ERR, "can't start send channel due to "
                           "WebRTC error (code %d)", voe_base_->LastError());
    
    started_ = true;
    DBG("started audio send channel");
    return RESULT_OK;
}

int NdnAudioSendChannel::stop()
{
    if (RESULT_FAIL(NdnAudioChannel::stop()))
        return RESULT_ERR;
    
    voe_base_->StopSend(channel_);
    voe_network_->DeRegisterExternalTransport(channel_);
    channel_ = -1;
    
    DBG("stopped audio send channel");
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - Transport
int NdnAudioSendChannel::SendPacket(int channel, const void *data, int len)
{
    TRACE("sending RTP packet of length %d", len);
    
    if (started_)
        audioSender_->publishRTPAudioPacket(len, (unsigned char*)data);
    else
        WARN("getting RTP packets while channel was not started");
    
    // return bytes sent or negative value on error
    return len;
}

int NdnAudioSendChannel::SendRTCPPacket(int channel, const void *data, int len)
{
    TRACE("sending RTCP packet of length %d", len);
    
    if (started_)
        audioSender_->publishRTCPAudioPacket(len, (unsigned char*)data);
    else
        WARN("getting RTCP packets while channel was not started");
    
    // return bytes sent or negative value on error
    return len;
}
