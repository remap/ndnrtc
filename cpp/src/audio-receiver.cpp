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
        FrameBuffer::Slot *slot = playoutBuffer_->acquireNextSlot();
        int framePlayoutTime = 0;
        int adjustedPlayoutTime = 0;
        bool isRTCP = false;
        
        jitterTiming_.startFramePlayout();
        
        if (slot)
        {
            NdnAudioData::AudioPacket packet = slot->getAudioFrame();
            
            if (packet.length_ && packet.data_)
            {
                isRTCP = packet.isRTCP_;
                
                if (packet.isRTCP_)
                    packetConsumer_->onRTCPPacketReceived(packet.length_,
                                                          packet.data_);
                else
                {
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
                
                // if av sync has been set up                
                if (slot && avSync_.get())
                    adjustedPlayoutTime= avSync_->synchronizePacket(slot,
                                                                    packetTsLocal);
            }
            else
                WARN("got bad audio packet");
        } // no slot - missed packet or underrun
        else
            WARN("can't obtain next audio slot");
        
        // get playout time
        framePlayoutTime = playoutBuffer_->releaseAcquiredSlot();
        framePlayoutTime += adjustedPlayoutTime;
        jitterTiming_.updatePlayoutTime(isRTCP?0:framePlayoutTime);
        
        // setup and run playout timer for calculated playout interval
        jitterTiming_.runPlayoutTimer();
    } // if packet consumer
}

bool NdnAudioReceiver::isLate(unsigned int frameNo)
{
    if (mode_ == ReceiverModeFetch &&
        frameNo < playoutBuffer_->framePointer())
        return true;
    
    return false;
}
