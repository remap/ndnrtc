//
//  audio-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING

#include "audio-receiver.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
NdnAudioReceiver::NdnAudioReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params)
{
    playoutBuffer_ = new PlayoutBuffer();
    frameLogger_ = new NdnLogger(NdnLoggerDetailLevelDefault,
                                 "fetch-astat-%s.log", params.producerId);
    
}

NdnAudioReceiver::~NdnAudioReceiver()
{
    stopFetching();
    delete frameLogger_;
}
//******************************************************************************
#pragma mark - public
int NdnAudioReceiver::init(shared_ptr<Face> face)
{
    return NdnMediaReceiver::init(face);
}

int NdnAudioReceiver::startFetching()
{
    return NdnMediaReceiver::startFetching();
}

int NdnAudioReceiver::stopFetching()
{
    return NdnMediaReceiver::stopFetching();
}

//******************************************************************************
#pragma mark - intefaces realization -

//******************************************************************************
#pragma mark - private
void NdnAudioReceiver::playbackPacket(int64_t packetTsLocal)
{
    if (packetConsumer_)
    {
        int framePlayoutTime = 0;
        
        FrameBuffer::Slot *slot = playoutBuffer_->acquireNextSlot();
        
        // after acquiring attempt, we could result in different state (i.e. if
        // rebuffering happened). check this
        if (mode_ == ReceiverModeFetch)
        {
            jitterTiming_.startFramePlayout();
            
            if (slot)
            {
                NdnAudioData::AudioPacket packet = slot->getAudioFrame();
                
                if (packet.length_ && packet.data_)
                {
                    if (packet.isRTCP_)
                        packetConsumer_->onRTCPPacketReceived(packet.length_,
                                                              packet.data_);
                    else
                    {
                        nPlayedOut_++;
                        packetConsumer_->onRTPPacketReceived(packet.length_,
                                                             packet.data_);
                        frameLogger_->log(NdnLoggerLevelInfo,
                                          "PLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld \t%ld \t%.2f",
                                          slot->getFrameNumber(),
                                          slot->assembledSegmentsNumber(),
                                          slot->totalSegmentsNumber(),
                                          slot->isKeyFrame(),
                                          playoutBuffer_->getJitterSize(),
                                          slot->getAssemblingTimeUsec(),
                                          packet.timestamp_,
                                          currentProducerRate_);
                    }
                }
                else
                    WARN("got bad audio packet");
            } // no slot - missed packet or underrun
            else
                WARN("can't obtain next audio slot");
            
            // get playout time
            framePlayoutTime = playoutBuffer_->releaseAcquiredSlot();
            
            int adjustedPlayoutTime = 0;
            // if av sync has been set up
            if (slot && avSync_.get())
                adjustedPlayoutTime = avSync_->synchronizePacket(slot,
                                                                 packetTsLocal,
                                                                 this);
            
            // check for error
            if (framePlayoutTime >= 0 && adjustedPlayoutTime >= 0)
            {
                framePlayoutTime += adjustedPlayoutTime;
                jitterTiming_.updatePlayoutTime(framePlayoutTime);
                
                // setup and run playout timer for calculated playout interval
                jitterTiming_.runPlayoutTimer();
            }
        }
    } // if packet consumer
}
