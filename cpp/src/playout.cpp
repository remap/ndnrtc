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
Playout::Playout(const shared_ptr<const Consumer> &consumer):
NdnRtcObject(consumer->getParameters()),
isRunning_(false),
consumer_(consumer),
playoutThread_(*webrtc::ThreadWrapper::CreateThread(Playout::playoutThreadRoutine, this)),
data_(nullptr)
{
    setDescription("playout");
    jitterTiming_.flush();
    
    if (consumer_.get())
    {
        frameBuffer_ = consumer_->getFrameBuffer();
    }
}

Playout::~Playout()
{
    if (isRunning_)
        stop();
}

//******************************************************************************
#pragma mark - public
int
Playout::init(void* frameConsumer)
{
    setDescription(description_);    
    jitterTiming_.flush();
    frameConsumer_ = frameConsumer;
    
    return RESULT_OK;
}

int
Playout::start()
{
    startPacketNo_ = 0;
    nPlayed_ = 0;
    nMissed_ = 0;
    latency_ = 0;
    isRunning_ = true;
    isInferredPlayback_ = false;
    lastPacketTs_ = 0;
    inferredDelay_ = 0;
    playbackAdjustment_ = 0;
    data_ = nullptr;
    
#if 1
    test_timelineDiff_ = -1;
#endif
    
    unsigned int tid;
    playoutThread_.Start(tid);
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
Playout::stop()
{
    playoutThread_.SetNotAlive();
    isRunning_ = false;
    playoutThread_.Stop();
    
    LogInfoC << "stopped" << endl;
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

void
Playout::getStatistics(ReceiverChannelPerformance& stat)
{
    stat.nPlayed_ = nPlayed_;
    stat.nMissed_ = nMissed_;
    stat.latency_ = latency_;
}

//******************************************************************************
#pragma mark - private
bool
Playout::processPlayout()
{
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
            
            frameBuffer_->acquireSlot(&data_, packetNo, sequencePacketNo,
                                      pairedPacketNo, isKey, assembledLevel);
            
            //******************************************************************
            // next call is overriden by specific playout mechanism - either
            // video or audio. the rest of the code is similar for both cases
            if (playbackPacket(now, data_, packetNo, sequencePacketNo,
                               pairedPacketNo, isKey, assembledLevel))
            {
                packetValid = true;
                
                nPlayed_++;
                updatePlaybackAdjustment();
            }
            else
            {
                nMissed_++;
                
                LogStatC
                << "\tskipping\t" << sequencePacketNo
                << "\ttotal\t" << nMissed_ << endl;
            }
#if 1
            // testing
            if (test_timelineDiff_ == -1 && data_)
            {
                test_timelineDiff_ = now-data_->getMetadata().timestamp_;
                test_timelineDiffInclineEst_ = NdnRtcUtils::setupInclineEstimator();
                
                LogTrace("timeslide.log")
                << "start timeline diff " << test_timelineDiff_ << endl;
            }
            else
            {
                if (data_)
                {
                    int currentDiffDelta = test_timelineDiff_ - (now-data_->getMetadata().timestamp_);
                    
                    NdnRtcUtils::inclineEstimatorNewValue(test_timelineDiffInclineEst_, (double)currentDiffDelta);
                    
                    
                    LogTrace("timeslide.log")
                    << "timeline diff delta for " << sequencePacketNo
                    << ": " << currentDiffDelta
                    << " incline " << NdnRtcUtils::currentIncline(test_timelineDiffInclineEst_)
                    << " now: " << now
                    << " ts: " << data_->getMetadata().timestamp_
                    << endl;
                }
            }
            // testing
#endif
            double frameUnixTimestamp = 0;

            if (data_)
            {
                frameUnixTimestamp = data_->getMetadata().unixTimestamp_;
                latency_ = NdnRtcUtils::unixTimestamp() - frameUnixTimestamp;
                LogStatC
                << "\tlatency\t" << latency_ << endl;
            }
            
            //******************************************************************
            // get playout time (delay) for the rendered frame
            int playbackDelay = frameBuffer_->releaseAcquiredSlot(isInferredPlayback_);
            
            adjustPlaybackDelay(playbackDelay);
            
            if (packetValid)
                playbackDelay += avSyncAdjustment(now, playbackDelay);
            
            assert(playbackDelay >= 0);
            
            LogTraceC
            << "packet " << sequencePacketNo << " has playout time (inferred "
            << (isInferredPlayback_?"YES":"NO") << ") "
            << playbackDelay
            << " (adjustment " << playbackAdjustment_
            << ") data: " << (data_?"YES":"NO") << endl;
            
            if (playbackDelay >= 0)
            {
                jitterTiming_.updatePlayoutTime(playbackDelay);
                
                // setup and run playout timer for calculated playout interval
                jitterTiming_.runPlayoutTimer();
            }
        }
    }
    
    return isRunning_;
}


void
Playout::updatePlaybackAdjustment()
{
    // check if previous frame playout time was inferred
    // it so - calculate adjustment
    if (lastPacketTs_ > 0 &&
        isInferredPlayback_)
    {
        int realPlayback = data_->getMetadata().timestamp_-lastPacketTs_;
        playbackAdjustment_ += (realPlayback-inferredDelay_);
        inferredDelay_ = 0;
        LogDebugC << "adjustments from previous iterations: " << playbackAdjustment_ << endl;
    }
    
    lastPacketTs_ = data_->getMetadata().timestamp_;
}

void
Playout::adjustPlaybackDelay(int& playbackDelay)
{
    if (isInferredPlayback_)
        inferredDelay_ += playbackDelay;
    else
        inferredDelay_ = 0;
    
    LogDebugC << "inferred delay " << inferredDelay_ << endl;
    
    if (playbackAdjustment_ < 0 &&
        abs(playbackAdjustment_) > playbackDelay)
    {
        playbackAdjustment_ += playbackDelay;
        playbackDelay = 0;
    }
    else
    {
        LogDebugC
        << "increased playback by adjustment: " << playbackDelay
        << " +" << playbackAdjustment_ << endl;
        
        playbackDelay += playbackAdjustment_;
        playbackAdjustment_ = 0;
    }
    LogDebugC << "playback adjustment:  " << playbackAdjustment_ << endl;
}

int
Playout::avSyncAdjustment(int64_t nowTimestamp, int playbackDelay)
{
    int syncDriftAdjustment = 0;
    
    if (consumer_->getAvSynchronizer().get())
    {
        syncDriftAdjustment = consumer_->getAvSynchronizer()->synchronizePacket(lastPacketTs_, nowTimestamp, (Consumer*)(consumer_.get()));
        
        LogTraceC << " av-sync adjustment: " << syncDriftAdjustment << endl;
        
        if (abs(syncDriftAdjustment) > playbackDelay && syncDriftAdjustment < 0)
        {
            playbackAdjustment_ = syncDriftAdjustment+playbackDelay;
            syncDriftAdjustment = -playbackDelay;
            LogTraceC << " av-sync updated playback adjustment: " << playbackAdjustment_ << endl;
        }
    }
    
    return syncDriftAdjustment;
}
