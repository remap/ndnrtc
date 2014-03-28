//
//  av-sync.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev

#include "av-sync.h"
#include "media-receiver.h"

using namespace webrtc;
using namespace ndnrtc;

#define SYNC_INIT(name) ({*CriticalSectionWrapper::CreateCriticalSection(), \
name, -1, -1, -1, -1, -1, -1})
#if 0
//******************************************************************************
#pragma mark - construction/destruction
AudioVideoSynchronizer::AudioVideoSynchronizer(ParamsStruct videoParams, ParamsStruct audioParams) :
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
audioSyncData_("audio"),
videoSyncData_("video"),
videoParams_(videoParams),
audioParams_(audioParams)
{
}

//******************************************************************************
#pragma mark - public
int AudioVideoSynchronizer::synchronizePacket(FrameBuffer::Slot *slot,
                                              int64_t packetTsLocal, NdnMediaReceiver *receiver)
{
    if (slot->isAudioPacket())
        return syncPacket(audioSyncData_, videoSyncData_,
                          slot->getPacketTimestamp(), packetTsLocal, receiver);
    
    if (slot->isVideoPacket())
        return syncPacket(videoSyncData_, audioSyncData_,
                          slot->getPacketTimestamp(), packetTsLocal, receiver);
    
    return 0;
}

void AudioVideoSynchronizer::reset()
{
    audioSyncData_.reset();
    videoSyncData_.reset();
    
    syncCs_.Enter();
    initialized_ = false;
    syncCs_.Leave();
}

//******************************************************************************
#pragma mark - private
int AudioVideoSynchronizer::syncPacket(SyncStruct& syncData,
                                       SyncStruct& pairedSyncData,
                                       int64_t packetTsRemote,
                                       int64_t packetTsLocal,
                                       NdnMediaReceiver *receiver)
{
    CriticalSectionScoped scopedCs(&syncCs_);
 
    syncData.cs_.Enter();
    
    TRACE("[SYNC] %s: synchronizing packet %ld (%ld)",
              syncData.name_, packetTsRemote, packetTsLocal);
    
    // check if it's a first call
    if (syncData.lastPacketTsLocal_ < 0)
        initialize(syncData, packetTsRemote, packetTsLocal, receiver);
    
    syncData.lastPacketTsLocal_ = packetTsLocal;
    syncData.lastPacketTsRemote_ = packetTsRemote;
    
    int drift = 0;
    
    if (initialized_)
    {
        CriticalSectionScoped scopedCs(&pairedSyncData.cs_);
        
        int64_t hitTime = packetTsLocal;
        
        // check with the paired stream - whether it's keeping up or not
        // 1. calcualte difference between remote and local timelines for the
        // paired stream
        int pairedD = pairedSyncData.lastPacketTsRemote_ - pairedSyncData.lastPacketTsLocal_;
        TRACE("[SYNC] %s: paired D is: %d", syncData.name_, pairedD);
        
        // 2. calculate hit time in remote's timeline of the paired stream
        int64_t hitTimeRemotePaired = hitTime + pairedD;
        TRACE("[SYNC] %s: hit remote is %d", syncData.name_, hitTimeRemotePaired);
        
        // 3. calculate drift by comparing remote times of the current stream
        // and hit time of the paired stream
        drift = syncData.lastPacketTsRemote_-hitTimeRemotePaired;
        TRACE("[SYNC] %s: drift is %d", syncData.name_, drift);
        
        unsigned int minJitterSize = (receiver == audioSyncData_.receiver_) ? audioParams_.jitterSize : videoParams_.jitterSize;
        
        // do not allow drift greater than jitter buffer size
        if (abs(drift) > minJitterSize)
        {
            // if drift > 0 - current stream is ahead, rebuffer paired stream
            if (drift > 0)
            {
                TRACE("[SYNC] %s: drift exceeded - rebuffering for %s",
                      syncData.name_, pairedSyncData.name_);
                drift = 0;
                pairedSyncData.receiver_->triggerRebuffering();
            }
            else
            {
                TRACE("[SYNC] %s: drift exceeded - rebuffering for %s",
                      syncData.name_, pairedSyncData.name_);
                syncData.receiver_->triggerRebuffering();
                drift = -1;
            }
        }
        else
        {
            if (drift > TolerableDriftMs)
                DBG("[SYNC] %s: adjusting %s stream for %d", syncData.name_,
                    pairedSyncData.name_, drift);
            else
                drift = 0;
        }
    } // critical section
    
    syncData.cs_.Leave();
    
    return drift;
}

void AudioVideoSynchronizer::onRebuffer(NdnMediaReceiver *receiver)
{
    if (audioSyncData_.receiver_ == receiver)
    {
        TRACE("audio channel encountered rebuffer. "
              "triggering video for rebuffering...");
        videoSyncData_.receiver_->triggerRebuffering();
    }
    else
    {
        TRACE("video channel encountered rebuffer. "
              "triggering audio for rebuffering...");
        audioSyncData_.receiver_->triggerRebuffering();
    }
}

// should be called from thread-safe place
void AudioVideoSynchronizer::initialize(SyncStruct &syncData,
                                        int64_t firstPacketTsRemote,
                                        int64_t localTimestamp,
                                        NdnMediaReceiver *receiver)
{
    TRACE("[SYNC] %s: initalize", syncData.name_);
    
    syncData.initialized_ = true;
    initialized_ = audioSyncData_.initialized_ && videoSyncData_.initialized_;
    
    syncData.receiver_ = receiver;
    syncData.lastPacketTsLocal_ = localTimestamp;
    syncData.lastPacketTsRemote_ = firstPacketTsRemote;
}

#endif