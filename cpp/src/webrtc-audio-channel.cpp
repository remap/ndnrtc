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
voeCodec_(nullptr),
voeNetwork_(nullptr)
{
}

WebrtcAudioChannel::~WebrtcAudioChannel()
{
    if (voeBase_)
        voeBase_->Release();

    if (voeCodec_)
        voeCodec_->Release();
    
    if (voeNetwork_)
        voeNetwork_->Release();
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
                                           //add high quality voice parameters (hard coded this time)
                                           voeCodec_ = VoECodec::GetInterface(NdnRtcUtils::sharedVoiceEngine());
                                           //set parameters  
                                           /*strcpy(cinst.plname, "ISAC");  
                                           cinst.plfreq   = 16000; // iSAC wideband sample rate
                                           cinst.pltype   = 105;   
                                           cinst.pacsize  = 480;   //use 30ms packet size，480kbps  
                                           cinst.channels = 2;     // stereo  */
                                           strcpy(cinst.plname, "opus");  //{120, "opus", 48000, 960, 2, 64000},
                                           cinst.plfreq   = 48000;
                                           cinst.pltype   = 120;   
                                           cinst.pacsize  = 960; 
                                           cinst.channels = 2; 
                                           //cinst.rate     = -1;    // network condition self-adapt
                                           cinst.rate     = 64000;
                                           webrtcChannelId_ = voeBase_->CreateChannel();
                                           //tell codec parameters
                                           voeCodec_->SetSendCodec(webrtcChannelId_, cinst);

                                           if (webrtcChannelId_ < 0)
                                               res = RESULT_ERR;
                                       });
    return res;
}
