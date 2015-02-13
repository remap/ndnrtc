//
//  playout.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#include "playout.h"
#include "ndnrtc-utils.h"
#include "av-sync.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
Playout::Playout(Consumer* consumer):
isRunning_(false),
consumer_(consumer),
playoutThread_(*webrtc::ThreadWrapper::CreateThread(Playout::playoutThreadRoutine, this)),
playoutCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()),
data_(nullptr),
observer_(nullptr)
{
    setDescription("playout");
    jitterTiming_.flush();
    
    if (consumer_)
    {
        frameBuffer_ = consumer_->getFrameBuffer();
    }
}

Playout::~Playout()
{
    if (isRunning_)
        stop();
    
    playoutThread_.~ThreadWrapper();
    playoutCs_.~CriticalSectionWrapper();
}

//******************************************************************************
#pragma mark - public
int
Playout::init(void* frameConsumer)
{
    setDescription(description_);
    frameConsumer_ = frameConsumer;
    
    return RESULT_OK;
}

int
Playout::start(int playbackAdjustment)
{
    webrtc::CriticalSectionScoped scopedCs_(&playoutCs_);
    
    jitterTiming_.flush();
    
    isRunning_ = true;
    isInferredPlayback_ = false;
    lastPacketTs_ = 0;
    inferredDelay_ = 0;
    playbackAdjustment_ = playbackAdjustment;
    data_ = nullptr;
    
    unsigned int tid;
    playoutThread_.Start(tid);
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
Playout::stop()
{
    webrtc::CriticalSectionScoped scopedCs_(&playoutCs_);
    if (isRunning_)
    {
        playoutThread_.SetNotAlive();
        isRunning_ = false;
        playoutThread_.Stop();
        jitterTiming_.stop();
        
        LogInfoC << "stopped" << endl;
    }
    else
        return RESULT_WARN;
    
    return RESULT_OK;
}

void
Playout::setLogger(ndnlog::new_api::Logger *logger)
{
    jitterTiming_.setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
Playout::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_.setDescription(NdnRtcUtils::toString("%s-timing",
                                                       getDescription().c_str()));
}

//******************************************************************************
#pragma mark - private
bool
Playout::processPlayout()
{
    playoutCs_.Enter();
    
    if (isRunning_)
    {
        int64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (frameBuffer_->getState() == FrameBuffer::Valid)
        {
            jitterTiming_.startFramePlayout();
            
            // cleanup from previous iteration
            if (data_)
            {
                delete data_;
                data_ = nullptr;
            }
            
            PacketNumber packetNo, sequencePacketNo, pairedPacketNo;
            double assembledLevel = 0;
            bool isKey, packetValid = false;
            bool skipped = false, missed = false,
                    outOfOrder = false, noData = false, incomplete = false;
            
            frameBuffer_->acquireSlot(&data_, packetNo, sequencePacketNo,
                                      pairedPacketNo, isKey, assembledLevel);
            
            if (observer_ && isKey)
                observer_->keyFrameConsumed();
            
            noData = (data_ == nullptr);
            incomplete = (assembledLevel < 1.);
            
            //******************************************************************
            // next call is overriden by specific playout mechanism - either
            // video or audio. the rest of the code is similar for both cases
            if (playbackPacket(now, data_, packetNo, sequencePacketNo,
                               pairedPacketNo, isKey, assembledLevel))
            {
                packetValid = true;
            }

            double frameUnixTimestamp = 0;

            if (data_)
            {
                updatePlaybackAdjustment();
                
                frameUnixTimestamp = data_->getMetadata().unixTimestamp_;
                stat_.latency_ = NdnRtcUtils::unixTimestamp() - frameUnixTimestamp;
                
                // update last packet timestamp if any
                if (data_->getMetadata().timestamp_ != -1)
                    lastPacketTs_ = data_->getMetadata().timestamp_;
            }
            
            //******************************************************************
            // get playout time (delay) for the rendered frame
            int playbackDelay = frameBuffer_->releaseAcquiredSlot(isInferredPlayback_);
            int adjustment = playbackDelayAdjustment(playbackDelay);
            int avSync = 0;
            
            if (packetValid)
                avSync = avSyncAdjustment(now, playbackDelay+adjustment);
            
            if (playbackDelay < 0)
            {
#warning this should be fixed with proper rate switching mechanism
                LogErrorC << "playback delay below zero: " << playbackDelay << endl;
                playbackDelay = 0;
            }
            assert(playbackDelay >= 0);
            
            LogTraceC << STAT_DIV
            << "packet" << STAT_DIV << sequencePacketNo << STAT_DIV
            << "lvl" << STAT_DIV << double(int(assembledLevel*10000))/100. << STAT_DIV
            << "played" << STAT_DIV << (packetValid?"YES":"NO") << STAT_DIV
            << "ts" << STAT_DIV << (noData ? 0 : data_->getMetadata().timestamp_) << STAT_DIV
            << "last ts" << STAT_DIV << lastPacketTs_ << STAT_DIV
            << "total" << STAT_DIV << playbackDelay+adjustment+avSync << STAT_DIV
            << "delay" << STAT_DIV << playbackDelay << STAT_DIV
            << "adjustment" STAT_DIV << adjustment << STAT_DIV
            << "avsync" << STAT_DIV << avSync << STAT_DIV
            << "inf delay" << STAT_DIV << inferredDelay_ << STAT_DIV
            << "inferred" << STAT_DIV << (isInferredPlayback_?"YES":"NO")
            << endl;
            
            playbackDelay += adjustment;
            if (!isInferredPlayback_)
                playbackDelay += avSync;
            
            assert(playbackDelay >= 0);

            if (observer_)
                isRunning_ = !observer_->recoveryCheck();
            playoutCs_.Leave();
            
            // setup and run playout timer for calculated playout interval            
            jitterTiming_.updatePlayoutTime(playbackDelay, sequencePacketNo);
            jitterTiming_.runPlayoutTimer();
        }
    }
    else
        playoutCs_.Leave();
    
    return isRunning_;
}


void
Playout::updatePlaybackAdjustment()
{
    // check if previous frame playout time was inferred
    // if so - calculate adjustment
    if (lastPacketTs_ > 0 &&
        isInferredPlayback_)
    {
        int realPlayback = data_->getMetadata().timestamp_-lastPacketTs_;
        playbackAdjustment_ += (realPlayback-inferredDelay_);
        inferredDelay_ = 0;
    }
}

int
Playout::playbackDelayAdjustment(int playbackDelay)
{
    int adjustment = 0;
    
    if (isInferredPlayback_)
        inferredDelay_ += playbackDelay;
    else
        inferredDelay_ = 0;
    
    if (playbackAdjustment_ < 0 &&
        abs(playbackAdjustment_) > playbackDelay)
    {
        playbackAdjustment_ += playbackDelay;
        adjustment = -playbackDelay;
    }
    else
    {
        adjustment = playbackAdjustment_;
        playbackAdjustment_ = 0;
    }
    
    LogTraceC << "updated adjustment " << playbackAdjustment_ << endl;
    
    return adjustment;
}

int
Playout::avSyncAdjustment(int64_t nowTimestamp, int playbackDelay)
{
    int syncDriftAdjustment = 0;
    
    if (consumer_->getAvSynchronizer().get())
    {
        syncDriftAdjustment = consumer_->getAvSynchronizer()->synchronizePacket(lastPacketTs_, nowTimestamp, (Consumer*)consumer_);
        
        if (abs(syncDriftAdjustment) > playbackDelay && syncDriftAdjustment < 0)
        {
            playbackAdjustment_ = syncDriftAdjustment+playbackDelay;
            syncDriftAdjustment = -playbackDelay;
        }
        if (syncDriftAdjustment > 0 &&
            syncDriftAdjustment > AudioVideoSynchronizer::MaxAllowableAvSyncAdjustment)
            syncDriftAdjustment = AudioVideoSynchronizer::MaxAllowableAvSyncAdjustment;
    }
    
    return syncDriftAdjustment;
}
