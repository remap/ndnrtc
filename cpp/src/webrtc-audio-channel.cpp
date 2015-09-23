//
//  webrtc-audio-channel.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include <boost/thread/mutex.hpp>

#include "webrtc-audio-channel.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

//******************************************************************************
#pragma mark - construction/destruction
WebrtcAudioChannel::WebrtcAudioChannel():
voeBase_(nullptr),
voeNetwork_(nullptr),
voeCodec_(nullptr)
{
}

WebrtcAudioChannel::~WebrtcAudioChannel()
{
    if (voeBase_)
        voeBase_->Release();
    
    if (voeNetwork_)
        voeNetwork_->Release();
    
    if (voeCodec_)
        voeCodec_->Release();
}

//******************************************************************************
#pragma mark - public
int WebrtcAudioChannel::init()
{
    int res = RESULT_OK;
    NdnRtcUtils::performOnBackgroundThread(
                                       [this, &res](){
                                           voeBase_ = VoEBase::GetInterface(NdnRtcUtils::sharedVoiceEngine());
                                           voeNetwork_ = VoENetwork::GetInterface(NdnRtcUtils::sharedVoiceEngine());
                                           voeCodec_ = VoECodec::GetInterface(NdnRtcUtils::sharedVoiceEngine());

                                           //initalize codec parameter struct
                                           CodecInst cinst;
                                           //set parameters
                                           strcpy(cinst.plname, "opus");
                                           cinst.plfreq   = 48000;
                                           cinst.pltype   = 120;
                                           cinst.pacsize  = 960;
                                           cinst.channels = 2;
                                           cinst.rate     = 128000;
                                           
                                           webrtcChannelId_ = voeBase_->CreateChannel();
                                           if (webrtcChannelId_ < 0)
                                               res = RESULT_ERR;
                                           
                                           voeCodec_->SetSendCodec(webrtcChannelId_, cinst);
                                       });
    return res;
}
