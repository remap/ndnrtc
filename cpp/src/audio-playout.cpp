//
//  audio-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "audio-playout.h"
#include "frame-data.h"
#include "frame-buffer.h"
#include "audio-renderer.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
AudioPlayout::AudioPlayout(Consumer* consumer,
                           const boost::shared_ptr<statistics::StatisticsStorage>& statStorage):
Playout(consumer, statStorage)
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
                             PacketNumber playbackPacketNo,
                             PacketNumber sequencePacketNo,
                             PacketNumber pairedPacketNo,
                             bool isKey, double assembledLevel)
{
    bool res = false;
    
    if (!data)
        (*statStorage_)[Indicator::SkippedIncompleteNum]++;
    
    if (data && frameConsumer_)
    {
        // unpack individual audio samples from audio data packet
        std::vector<NdnAudioData::AudioPacket> audioSamples =
        ((NdnAudioData*)data)->getPackets();
        
        std::vector<NdnAudioData::AudioPacket>::iterator it;
        
        for (it = audioSamples.begin(); it != audioSamples.end(); ++it)
        {
            NdnAudioData::AudioPacket audioSample = *it;
            
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
        }
        
        // update stat
        (*statStorage_)[Indicator::PlayedNum]++;
        
        res = true;
    }
    
    return res;
}
