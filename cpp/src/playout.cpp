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


#include <boost/bind.hpp>

#include "playout.h"
#include "jitter-timing.h"
#include "simple-log.h"
#include "frame-buffer.h"
#include "frame-data.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api::statistics;

//******************************************************************************
Playout::Playout(boost::asio::io_service& io,
    const boost::shared_ptr<PlaybackQueue>& queue,
    const boost::shared_ptr<new_api::statistics::StatisticsStorage> statStorage):
jitterTiming_(io),
pqueue_(queue),
StatObject(statStorage),
lastTimestamp_(-1),
lastDelay_(-1),
delayAdjustment_(0)
{
    setDescription("playout");
}

Playout::~Playout()
{
    if (isRunning_)
        stop();
}

void
Playout::start(unsigned int fastForwardMs)
{
    if (isRunning_)
        throw std::runtime_error("Playout has started already");

    jitterTiming_.flush();
    delayAdjustment_ = fastForwardMs;
    isRunning_ = true;
    extractSample();

    LogInfoC << "started" << std::endl;
}

void
Playout::stop()
{
    if (isRunning_)
    {
        isRunning_ = false;
        lastTimestamp_ = -1;
        lastDelay_ = -1;
        jitterTiming_.stop();
        LogInfoC << "stopped" << std::endl;
    }
}

void
Playout::setLogger(ndnlog::new_api::Logger* logger)
{
    ILoggingObject::setLogger(logger);
    jitterTiming_.setLogger(logger);
}

void
Playout::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_.setDescription(getDescription()+"-timing");
}

void 
Playout::attach(IPlayoutObserver* o) 
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    observers_.push_back(o); 
}

void 
Playout::detach(IPlayoutObserver* o) 
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    observers_.erase(std::find(observers_.begin(), observers_.end(), o));
}

#pragma mark - private
void Playout::extractSample()
{
    if (!isRunning_) return;
    
    int64_t sampleDelay = (int64_t)round(pqueue_->samplePeriod());
    jitterTiming_.startFramePlayout();

    if (pqueue_->size())
    {
        pqueue_->pop([this, &sampleDelay](const boost::shared_ptr<const BufferSlot>& slot, double playTimeMs){
            processSample(slot);
            correctAdjustment(slot->getHeader().publishTimestampMs_);
            lastTimestamp_ = slot->getHeader().publishTimestampMs_;
            sampleDelay = playTimeMs;
        });

        LogTraceC << ". packet delay " << sampleDelay << " ts " << lastTimestamp_ << std::endl;
    }
    else
    {
        LogWarnC << "playback queue is empty" << std::endl;
        {
            boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
            for (auto o:observers_) o->onQueueEmpty();
        }
    }

    jitterTiming_.updatePlayoutTime(adjustDelay(sampleDelay));
    jitterTiming_.run(boost::bind(&Playout::extractSample, this));
}

void Playout::correctAdjustment(int64_t newSampleTimestamp)
{
    if (lastDelay_ >= 0)
    {
        int64_t timestamp = newSampleTimestamp;
        int64_t prevDelay = timestamp-lastTimestamp_;

        delayAdjustment_ += (prevDelay - lastDelay_);
    }
}

int64_t Playout::adjustDelay(int64_t delay)
{
    int64_t adj = 0;

    if (delayAdjustment_ < 0 &&
        abs(delayAdjustment_) > delay)
    {
        delayAdjustment_ += delay;
        adj = -delay;
    }
    else
    {
        adj = delayAdjustment_;
        delayAdjustment_ = 0;
    }

    LogTraceC << ". total adj " << delayAdjustment_ << " delay " 
    << (delay+adj) << std::endl;

    return (delay+adj);
}

#if 0
//******************************************************************************
#pragma mark - construction/destruction
Playout::Playout(Consumer* consumer,
                 const boost::shared_ptr<StatisticsStorage>& statStorage):
StatObject(statStorage),
jitterTiming_(new JitterTiming()),
isRunning_(false),
consumer_(consumer),
data_(nullptr)
{
    setDescription("playout");
    jitterTiming_->flush();
    
    if (consumer_)
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
    frameConsumer_ = frameConsumer;
    
    return RESULT_OK;
}

int
Playout::start(int initialAdjustment)
{
    {
        lock_guard<mutex> scopeLock(playoutMutex_);
        
        jitterTiming_->flush();
        
        isRunning_ = true;
        isInferredPlayback_ = false;
        lastPacketTs_ = 0;
        inferredDelay_ = 0;
        playbackAdjustment_ = initialAdjustment;
    }
    
    NdnRtcUtils::dispatchOnBackgroundThread([this](){
        processPlayout();
    });
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
Playout::stop()
{
    lock_guard<mutex> scopeLock(playoutMutex_);
    
    if (isRunning_)
    {
        isRunning_ = false;
        jitterTiming_->stop();
        
        LogInfoC << "stopped" << endl;
    }
    else
        return RESULT_WARN;
    
    if (data_)
    {
        delete data_;
        data_ = nullptr;
    }
    
    return RESULT_OK;
}

void
Playout::setLogger(ndnlog::new_api::Logger *logger)
{
    jitterTiming_->setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
Playout::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_->setDescription(NdnRtcUtils::toString("%s-timing",
                                                       getDescription().c_str()));
}

//******************************************************************************
#pragma mark - private
bool
Playout::processPlayout()
{
    playoutMutex_.lock();
    
    if (isRunning_)
    {
        int64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (frameBuffer_->getState() == FrameBuffer::Valid)
        {
            jitterTiming_->startFramePlayout();
            
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
            
            noData = (data_ == nullptr);
            incomplete = (assembledLevel < 1.);
            
            //******************************************************************
            // next call is overriden by specific playout mechanism - either
            // video or audio. the rest of the code is similar for both cases
            if (playbackPacket(now, data_, packetNo, sequencePacketNo,
                               pairedPacketNo, isKey, assembledLevel))
            {
                packetValid = true;
                (*statStorage_)[Indicator::LastPlayedNo] = packetNo;
                (*statStorage_)[Indicator::LastPlayedDeltaNo] = (isKey)?pairedPacketNo:sequencePacketNo;
                (*statStorage_)[Indicator::LastPlayedKeyNo] = (isKey)?sequencePacketNo:pairedPacketNo;
            }

            double frameUnixTimestamp = 0;

            if (data_)
            {
                updatePlaybackAdjustment();
                
                frameUnixTimestamp = data_->getMetadata().unixTimestamp_;
                (*statStorage_)[Indicator::LatencyEstimated] = NdnRtcUtils::unixTimestamp() - frameUnixTimestamp;
                
                LogStatC << "lat est" << STAT_DIV << std::fixed << std::setprecision(5)
                << (*statStorage_)[Indicator::LatencyEstimated] << std::endl;
                
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
                // should never happen
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
            << "total adj" << STAT_DIV << playbackAdjustment_ << STAT_DIV
            << "inf delay" << STAT_DIV << inferredDelay_ << STAT_DIV
            << "inferred" << STAT_DIV << (isInferredPlayback_?"YES":"NO")
            << endl;
            
            playbackDelay += adjustment;
            if (!isInferredPlayback_)
                playbackDelay += avSync;
            
            assert(playbackDelay >= 0);
            playoutMutex_.unlock();
            
            if (isRunning_)
            {
                // setup and run playout timer for calculated playout interval
                jitterTiming_->updatePlayoutTime(playbackDelay, sequencePacketNo);
                jitterTiming_->run(boost::bind(&Playout::processPlayout, this));
            }
        }
    }
    else
        playoutMutex_.unlock();
    
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
    
    LogTraceC << "updated total adj. " << playbackAdjustment_ << endl;
    
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
#endif
