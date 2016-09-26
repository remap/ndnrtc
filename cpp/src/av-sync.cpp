//
//  av-sync.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev

#include "av-sync.hpp"
#if 0
#include "consumer.hpp"

using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;
using namespace boost;

const int64_t AudioVideoSynchronizer::TolerableLeadingDriftMs = 15;
const int64_t AudioVideoSynchronizer::TolerableLaggingDriftMs = 45;
const int64_t AudioVideoSynchronizer::MaxAllowableAvSyncAdjustment = 50;

//******************************************************************************
#pragma mark - construction/destruction
AudioVideoSynchronizer::AudioVideoSynchronizer(const shared_ptr<new_api::Consumer>& masterConsumer,
                       const shared_ptr<new_api::Consumer>& slaveConsumer):
slaveSyncData_("slave"),
masterSyncData_("master")
{
    description_ = "av-sync";
    
    if (masterConsumer.get())
    {
        masterSyncData_.consumer_ = masterConsumer.get();
    }
    
    if (slaveConsumer.get())
    {
        slaveSyncData_.consumer_ = slaveConsumer.get();
    }
}

AudioVideoSynchronizer::~AudioVideoSynchronizer(){
}

//******************************************************************************
#pragma mark - public
int AudioVideoSynchronizer::synchronizePacket(int64_t remoteTimestamp,
                                              int64_t packetTsLocal,
                                              Consumer *consumer)
{
    if (consumer == slaveSyncData_.consumer_)
        return syncPacket(slaveSyncData_, masterSyncData_,
                          remoteTimestamp, packetTsLocal, consumer);
    
    if (consumer == masterSyncData_.consumer_)
        return syncPacket(masterSyncData_, slaveSyncData_,
                          remoteTimestamp, packetTsLocal, consumer);
    
    return 0;
}

void AudioVideoSynchronizer::reset()
{
    slaveSyncData_.reset();
    masterSyncData_.reset();
    
    {
        lock_guard<mutex> scopedLock(syncMutex_);
        initialized_ = false;
    }
}

//******************************************************************************
#pragma mark - private
int AudioVideoSynchronizer::syncPacket(SyncStruct& syncData,
                                       SyncStruct& pairedSyncData,
                                       int64_t packetTsRemote,
                                       int64_t packetTsLocal,
                                       Consumer *consumer)
{
    lock_guard<mutex> scopedLock(syncMutex_);
 
    syncData.mutex_.lock();
    
    // check if it's a first call
    if (syncData.lastPacketTsLocal_ < 0)
        initialize(syncData, packetTsRemote, packetTsLocal, consumer);
    
    syncData.lastPacketTsLocal_ = packetTsLocal;
    syncData.lastPacketTsRemote_ = packetTsRemote;
    
    int drift = 0;
    
    // we synchronize audio only
    if (consumer == this->slaveSyncData_.consumer_ && initialized_)
    {
        lock_guard<mutex> scopedLock2(pairedSyncData.mutex_);
        int64_t hitTime = packetTsLocal;
        
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
        << " pairedD is " << pairedD << std::endl;
        
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
        << " hit remote is " << hitTimeRemotePaired << std::endl;
        
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
        << " drift is " << drift << std::endl;
        
        // check drift value
        if ((drift > 0 && drift < TolerableLeadingDriftMs) ||
            (drift < 0 && drift > -TolerableLaggingDriftMs))
        {
            drift = 0;
        }
        
    } // critical section
    
    syncData.mutex_.unlock();
    
    return drift;
}

void AudioVideoSynchronizer::onRebuffer(Consumer *consumer)
{
    if (slaveSyncData_.consumer_ == consumer)
    {
        LogTraceC << "audio channel encountered rebuffer. "
        "triggering video for rebuffering..." << std::endl;
        
        masterSyncData_.consumer_->triggerRebuffering();
    }
    else
    {
        LogTraceC << "video channel encountered rebuffer. "
        "triggering audio for rebuffering..." << std::endl;
        
        slaveSyncData_.consumer_->triggerRebuffering();
    }
}

// should be called from thread-safe place
void AudioVideoSynchronizer::initialize(SyncStruct &syncData,
                                        int64_t firstPacketTsRemote,
                                        int64_t localTimestamp,
                                        Consumer *consumer)
{
    LogTraceC << "initalize " << syncData.name_ << std::endl;
    
    syncData.initialized_ = true;
    initialized_ = slaveSyncData_.initialized_ && masterSyncData_.initialized_;
    
    syncData.consumer_ = consumer;
    syncData.lastPacketTsLocal_ = localTimestamp;
    syncData.lastPacketTsRemote_ = firstPacketTsRemote;
}
#endif