//
//  av-sync.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 1/17/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "av-sync.h"

using namespace ndnrtc;

//******************************************************************************
#pragma mark - construction/destruction
AudioVideoSynchronizer::AudioVideoSynchronizer(double audioRate,
                                               double videoRate)
{
    updateRate(audioSyncData_, audioRate);
    updateRate(videoSyncData_, videoRate);
}

//******************************************************************************
#pragma mark - public
void AudioVideoSynchronizer::setSynchronizationFrequency(double syncFrequency)
{
    syncFrequency_ = syncFrequency;
}

int AudioVideoSynchronizer::synchronizePacket(FrameBuffer::Slot *slot,
                                          int playoutDuration)
{
    if (slot->isAudioPacket())
        return syncPacket(audioSyncData_, slot, playoutDuration);
    
    if (slot->isVideoPacket())
        return syncPacket(videoSyncData_, slot, playoutDuration);
    
    return 0;
}

//******************************************************************************
#pragma mark - private
int AudioVideoSynchronizer::syncPacket(SyncStruct& syncData,
                                       FrameBuffer::Slot* slot,
                                       int playoutDuration)
{
    // check if it's a first call
    if (syncData.playbackStartLocal_ < 0)
        initialize(syncData, slot, playoutDuration);
    
    return 0;
}

void AudioVideoSynchronizer::updateRate(SyncStruct &syncData, double rate)
{
    if (rate != 0 && rate != syncData.actualPacketRate_)
        syncData.actualPacketRate_ = rate;
}

void AudioVideoSynchronizer::initialize(SyncStruct &syncData,
                                        FrameBuffer::Slot *slot,
                                        int playoutDuration)
{
    // TBD
}


