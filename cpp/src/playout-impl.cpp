// 
// playout-impl.cpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "playout-impl.hpp"

#include "playout.hpp"
#include "jitter-timing.hpp"
#include "simple-log.hpp"
#include "frame-buffer.hpp"
#include "frame-data.hpp"
#include "clock.hpp"

using namespace ndnrtc;
using namespace std;
using namespace ndnrtc::statistics;

PlayoutImpl::PlayoutImpl(boost::asio::io_service& io,
    const boost::shared_ptr<IPlaybackQueue>& queue,
    const boost::shared_ptr<statistics::StatisticsStorage> statStorage):
isRunning_(false),
jitterTiming_(io),
pqueue_(queue),
StatObject(statStorage),
lastTimestamp_(-1),
lastDelay_(-1),
delayAdjustment_(0)
{
    setDescription("playout");
}

PlayoutImpl::~PlayoutImpl()
{
    if (isRunning_)
        stop();
}

void
PlayoutImpl::start(unsigned int fastForwardMs)
{
    if (isRunning_)
        throw std::runtime_error("Playout has started already");

    jitterTiming_.flush();
    lastTimestamp_ = -1;
    lastDelay_ = -1;
    delayAdjustment_ = -(int)fastForwardMs;
    isRunning_ = true;
    
    LogInfoC << "started (‣‣" << fastForwardMs << "ms)" << std::endl;
    extractSample();
}

void
PlayoutImpl::stop()
{
    if (isRunning_)
    {
        isRunning_ = false;
        jitterTiming_.stop();
        LogInfoC << "stopped" << std::endl;
    }
}

void
PlayoutImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    ILoggingObject::setLogger(logger);
    jitterTiming_.setLogger(logger);
}

void
PlayoutImpl::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_.setDescription(getDescription()+"-timing");
}

void 
PlayoutImpl::attach(IPlayoutObserver* o) 
{
    if (o)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        observers_.push_back(o);
    }
}

void
PlayoutImpl::detach(IPlayoutObserver* o) 
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    observers_.erase(std::find(observers_.begin(), observers_.end(), o));
}

#pragma mark - private
void PlayoutImpl::extractSample()
{
    if (!isRunning_) return;
    
    stringstream debugStr;
    int64_t sampleDelay = (int64_t)round(pqueue_->samplePeriod());
    jitterTiming_.startFramePlayout();

    if (pqueue_->size())
    {
        pqueue_->pop([this, &sampleDelay, &debugStr](const boost::shared_ptr<const BufferSlot>& slot, double playTimeMs){
            processSample(slot);
            correctAdjustment(slot->getHeader().publishTimestampMs_);
            lastTimestamp_ = slot->getHeader().publishTimestampMs_;
            sampleDelay = playTimeMs;
            debugStr << slot->dump();
            (*statStorage_)[Indicator::LatencyEstimated] = (clock::unixTimestamp() - slot->getHeader().publishUnixTimestamp_);
        });

        LogTraceC << ". packet delay " << sampleDelay << " ts " << lastTimestamp_ << std::endl;
    }
    else
    {
        lastTimestamp_ += sampleDelay;
        LogWarnC << "playback queue is empty" << std::endl;
        {
            boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
            for (auto o:observers_) o->onQueueEmpty();
        }
    }

    lastDelay_ = sampleDelay;
    int64_t actualDelay = adjustDelay(sampleDelay);

    LogDebugC << "●--" << debugStr.str() << actualDelay << "ms" << std::endl;

    boost::shared_ptr<PlayoutImpl> me = boost::dynamic_pointer_cast<PlayoutImpl>(shared_from_this());
    jitterTiming_.updatePlayoutTime(actualDelay);
    jitterTiming_.run(boost::bind(&PlayoutImpl::extractSample, me));
}

void PlayoutImpl::correctAdjustment(int64_t newSampleTimestamp)
{
    if (lastDelay_ >= 0)
    {
        int64_t prevDelay = newSampleTimestamp-lastTimestamp_;
        
        LogTraceC << ". prev delay hard " << prevDelay 
            << " offset " << prevDelay - lastDelay_ << std::endl;

        delayAdjustment_ += (prevDelay - lastDelay_);
    }
}

int64_t PlayoutImpl::adjustDelay(int64_t delay)
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
