//
//  audio-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "audio-playout.h"
#include "frame-data.h"
#include "frame-buffer.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
AudioPlayout::AudioPlayout(const shared_ptr<const Consumer>& consumer):
Playout(consumer)
{
    description_ = "audio-playout";
}

AudioPlayout::~AudioPlayout()
{
    
}

//******************************************************************************
#pragma mark - public

//******************************************************************************
#pragma mark - private
bool
AudioPlayout::playbackPacket(int64_t packetTsLocal, PacketData* data,
                             PacketNumber packetNo, bool isKey)
{
    bool res = false;
    
    NdnAudioData::AudioPacket audioSample;
    
    if (data && frameConsumer_)
    {
        ((NdnAudioData*)data)->getAudioPacket(audioSample);
        
        if (audioSample.isRTCP_)
        {
            ((AudioRenderer*)frameConsumer_)->onDeliverRtcpFrame(audioSample.length_,
                                                                       audioSample.data_);
        }
        else
        {
            ((AudioRenderer*)frameConsumer_)->onDeliverRtpFrame(audioSample.length_,
                                                                      audioSample.data_);
        }
        
        res = true;
    }
    
    return res;
}
