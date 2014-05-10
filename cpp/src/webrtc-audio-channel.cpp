//
//  webrtc-audio-channel.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "webrtc-audio-channel.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;

//******************************************************************************
#pragma mark - construction/destruction
WebrtcAudioChannel::WebrtcAudioChannel(webrtc::VoiceEngine* voiceEngine):
voeBase_(VoEBase::GetInterface(voiceEngine)),
voeNetwork_(VoENetwork::GetInterface(voiceEngine))
{
    
}

WebrtcAudioChannel::~WebrtcAudioChannel()
{
    voeBase_->Release();
    voeNetwork_->Release();
}

//******************************************************************************
#pragma mark - public
int WebrtcAudioChannel::init()
{
    webrtcChannelId_ = voeBase_->CreateChannel();
    
    if (webrtcChannelId_ < 0)
        return RESULT_ERR;
    
    return RESULT_OK;
}
