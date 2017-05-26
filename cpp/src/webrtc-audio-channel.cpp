//
//  webrtc-audio-channel.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include <boost/thread/mutex.hpp>
#include <webrtc/common_types.h>
#include <webrtc/voice_engine/include/voe_base.h>
#include <webrtc/voice_engine/include/voe_network.h>
#include <webrtc/voice_engine/include/voe_codec.h>
#include <algorithm>

#include "ndnrtc-defines.hpp"
#include "webrtc-audio-channel.hpp"
#include "audio-controller.hpp"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
#pragma mark - construction/destruction
WebrtcAudioChannel::WebrtcAudioChannel(const Codec& codec):
voeBase_(nullptr),
voeNetwork_(nullptr),
voeCodec_(nullptr)
{
  int res = 0;

  AudioController::getSharedInstance()->initVoiceEngine();
  AudioController::getSharedInstance()->performOnAudioThread(
    [this, &res, codec](){
      voeBase_ = VoEBase::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());
      voeNetwork_ = VoENetwork::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());
      voeCodec_ = VoECodec::GetInterface(AudioController::getSharedInstance()->getVoiceEngine());

      webrtcChannelId_ = voeBase_->CreateChannel();
      if (webrtcChannelId_ < 0)
        res = RESULT_ERR;

      voeCodec_->SetSendCodec(webrtcChannelId_, WebrtcAudioChannel::instFromCodec(codec));
    });
  if (RESULT_FAIL(res))
    throw std::runtime_error("Can't create audio channel");
}

WebrtcAudioChannel::~WebrtcAudioChannel()
{
  AudioController::getSharedInstance()->performOnAudioThread([this](){
    voeBase_->DeleteChannel(webrtcChannelId_);
    voeBase_->Release();
    voeNetwork_->Release();
    voeCodec_->Release();
  });
}

WebrtcAudioChannel::Codec
WebrtcAudioChannel::fromString(std::string codec)
{
    std::transform(codec.begin(), codec.end(), codec.begin(), ::tolower);
    if (codec == "opus")
        return WebrtcAudioChannel::Codec::Opus;
    return WebrtcAudioChannel::Codec::G722;
}

CodecInst
WebrtcAudioChannel::instFromCodec(const Codec& c)
{
  CodecInst cinst;

  switch (c)
  {
    case Codec::Opus:
      {
        strcpy(cinst.plname, "opus");
        cinst.plfreq   = 48000;
        cinst.pltype   = 120;
        cinst.pacsize  = 960;
        cinst.channels = 2;
        cinst.rate     = 128000;
      }
      break;
    case Codec::G722: // fall through
    default:
      {
        strcpy(cinst.plname, "G722");
        cinst.plfreq   = 16000;
        cinst.pltype   = 119;
        cinst.pacsize  = 320;
        cinst.channels = 2;
        cinst.rate     = 64000;
      }
      break;
  }
  return cinst;
}
