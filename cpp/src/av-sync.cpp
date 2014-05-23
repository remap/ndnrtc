//
//  av-sync.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev

#include "av-sync.h"

using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace std;

#define SYNC_INIT(name) ({*CriticalSectionWrapper::CreateCriticalSection(), \
name, -1, -1, -1, -1, -1, -1})

//******************************************************************************
#pragma mark - construction/destruction
AudioVideoSynchronizer::AudioVideoSynchronizer(const shared_ptr<new_api::VideoConsumer>& videoConsumer,
                       const shared_ptr<new_api::AudioConsumer>& audioConsumer):
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
audioSyncData_("audio"),
videoSyncData_("video")
{
    description_ = "av-sync";
    
    if (videoConsumer.get())
    {
        videoSyncData_.consumer_ = videoConsumer.get();
        videoParams_ = videoConsumer->getParameters();
    }
    
    if (audioConsumer.get())
    {
        audioSyncData_.consumer_ = audioConsumer.get();
        audioParams_ = audioConsumer->getParameters();
    }
}

//******************************************************************************
#pragma mark - public
int AudioVideoSynchronizer::synchronizePacket(int64_t remoteTimestamp,
                                              int64_t packetTsLocal,
                                              Consumer *consumer)
{
    if (consumer == audioSyncData_.consumer_)
        return syncPacket(audioSyncData_, videoSyncData_,
                          remoteTimestamp, packetTsLocal, consumer);
    
    if (consumer == videoSyncData_.consumer_)
        return syncPacket(videoSyncData_, audioSyncData_,
                          remoteTimestamp, packetTsLocal, consumer);
    
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
                                       Consumer *consumer)
{
    CriticalSectionScoped scopedCs(&syncCs_);
 
    syncData.cs_.Enter();
    
    LogTraceC << syncData.name_
    << " synchronizing packet " << packetTsRemote <<  packetTsLocal << endl;
    
    // check if it's a first call
    if (syncData.lastPacketTsLocal_ < 0)
        initialize(syncData, packetTsRemote, packetTsLocal, consumer);
    
    syncData.lastPacketTsLocal_ = packetTsLocal;
    syncData.lastPacketTsRemote_ = packetTsRemote;
    
    int drift = 0;
    
    // we synchronize audio only
    if (consumer == this->audioSyncData_.consumer_ && initialized_)
    {
        CriticalSectionScoped scopedCs(&pairedSyncData.cs_);
        
        int64_t hitTime = packetTsLocal;
        
        LogTrace("media-diff.log") << "\tdiff\t" << syncData.lastPacketTsRemote_ - pairedSyncData.lastPacketTsRemote_ << endl;
        
        // packet synchroniztation comes from the idea of strem timelines:
        // each media stream has timeline with points which corresponds to the
        // media packet:
        //  ex.:
        //      video timeline (CLCK_R)          audio timeline (CLCK_R)
        //          |                               |
        //      t1v-+ - video frame                 |
        //          |   ¡                       t1a-+ - audio sample
        //          |   |  inter-frame delay        |   ¡ inter-sample delay
        //          |   !                           |   !
        //      t2v-+ - video frame             t2a-+ - audio sample
        //          |                               |
        //          |                               |
        //          |                           t3a-+ - audio sample
        //      t3v-+ - video frame                 |
        //          |                               |
        //
        // - timelines are based on the same remote clock (CLCK_R)
        // - timelines are played out using local clock (CLCK_L)
        // - synchronization is therefore consists of determining offset between
        //      two timeline and adjusting them by delaying leading media stream
        // - further in the comments:
        //      sync'ed media - newly received packet of the media being checked
        //      paired media - other media stream against which sync'ed media is
        //                      being checked
        
        // check with the paired stream - whether it's keeping up or not
        // 1. calcualte difference between remote and local timelines for the
        // paired stream
        // (in this example, TsL and T1 should be synchronized - sync'ed media
        // need to be delayed)
        //
        //      paired media                    paired media                    sync'ed media                           sync'ed media
        //    (local timeline)             (remote timeline)                    (local timeline)                        (remote timeline)
        //          (CLCK_L)                    (CLCK_R)                            (CLCK_L)                            (CLCK_R)
        //              |                           |                                   |                                   |
        //              |                           |                                   +                                   |
        // last packet  + <- timestmp local(TsL)    + <- timestamp remote (TsR)         |                               T1  +
        //              :                           :                                   |                                   |
        //              :                           :                              T1   + <- hitTime (timestamp local)      |
        // pairedD = TsR - TsL
        //
        int pairedD = pairedSyncData.lastPacketTsRemote_ - pairedSyncData.lastPacketTsLocal_;
        LogTraceC << syncData.name_
        << " pairedD is " << pairedD << endl;
        
        // 2. calculate hit time in remote's timeline of the paired stream
        //
        //      paired media                    paired media                    sync'ed media
        //    (local timeline)             (remote timeline)                    (local timeline)
        //          (CLCK_L)                    (CLCK_R)                            (CLCK_L)
        //              |                           |                                   |
        //              |                           |                                   +
        // last packet  +                           +                                   |
        //              :                           :    hitRemote = hitTime + pairedD  |
        //              :                           : <................................ + <- hitTime (timestamp local)
        
        int64_t hitTimeRemotePaired = hitTime + pairedD;
        LogTraceC << syncData.name_
        << " hit remote is " << hitTimeRemotePaired << endl;
        
        // 3. calculate drift by comparing remote times of the current stream
        // and hit time of the paired stream
        //
        //         paired media                       sync'ed media            sync'ed media
        //      (remote timeline)                    (local timeline)          (remote timeline)
        //          (CLCK_R)                            (CLCK_L)                (CLCK_R)
        //              |                                   |                       |
        //              |                                   +                       |
        //              +                                   |                       + <- timestamp remote ......
        //              :    hitRemote = hitTime + pairedD  |                       |                           } - drift
        // hitRemote -> : <................................ + <- hitTime........................................
        //
        // positive drift - sync'ed media leads paired media (audio leads video)
        // negative drift - sync'de media lags paired media (audio lags video)
        drift = syncData.lastPacketTsRemote_-hitTimeRemotePaired;
        
        
        
        LogTraceC << syncData.name_
        << " drift is " << drift << endl;
        
//        unsigned int minJitterSize = (consumer == audioSyncData_.consumer_) ? audioParams_.jitterSize : videoParams_.jitterSize;
        
        // do not allow drift greater than jitter buffer size
        if ((drift > 0 && drift < TolerableLeadingDriftMs) ||
            (drift < 0 && drift > -TolerableLaggingDriftMs))
        {
            drift = 0;
        }
//        {
//            // if drift > 0 - current stream is ahead, rebuffer paired stream
//            if (drift > 0)
//            {
//                LogTraceC << syncData.name_
//                << " drift exceeded - rebuffering for "
//                << pairedSyncData.name_ << endl;
//                
//                drift = 0;
//                pairedSyncData.consumer_->triggerRebuffering();
//            }
//            else
//            {
//                LogTraceC << syncData.name_
//                << " drift exceeded - rebuffering for "
//                << syncData.name_ << endl;
//                
//                syncData.consumer_->triggerRebuffering();
//                drift = 0;
//            }
//        }
//        else
//        {
//            if (drift > TolerableDriftMs)
//            {
//                LogDebugC << syncData.name_
//                << " adjusting %s stream for %d" << pairedSyncData.name_
//                <<  drift << endl;
//            }
//            else
//                drift = 0;
//        }        
    } // critical section
    
    syncData.cs_.Leave();
    
    return drift;
}

void AudioVideoSynchronizer::onRebuffer(Consumer *consumer)
{
    if (audioSyncData_.consumer_ == consumer)
    {
        LogTraceC << "audio channel encountered rebuffer. "
        "triggering video for rebuffering..." << endl;
        
        videoSyncData_.consumer_->triggerRebuffering();
    }
    else
    {
        LogTraceC << "video channel encountered rebuffer. "
              "triggering audio for rebuffering..." << endl;
        
        audioSyncData_.consumer_->triggerRebuffering();
    }
}

// should be called from thread-safe place
void AudioVideoSynchronizer::initialize(SyncStruct &syncData,
                                        int64_t firstPacketTsRemote,
                                        int64_t localTimestamp,
                                        Consumer *consumer)
{
    LogTraceC << "initalize " << syncData.name_ << endl;
    
    syncData.initialized_ = true;
    initialized_ = audioSyncData_.initialized_ && videoSyncData_.initialized_;
    
    syncData.consumer_ = consumer;
    syncData.lastPacketTsLocal_ = localTimestamp;
    syncData.lastPacketTsRemote_ = firstPacketTsRemote;
}
