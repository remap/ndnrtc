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
    lastTimestamp_ = -1;
    lastDelay_ = -1;
    delayAdjustment_ = fastForwardMs;
    isRunning_ = true;
    
    LogInfoC << "started" << std::endl;
    extractSample();
}

void
Playout::stop()
{
    if (isRunning_)
    {
        isRunning_ = false;
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

    jitterTiming_.updatePlayoutTime(actualDelay);
    jitterTiming_.run(boost::bind(&Playout::extractSample, this));
}

void Playout::correctAdjustment(int64_t newSampleTimestamp)
{
    if (lastDelay_ >= 0)
    {
        int64_t prevDelay = newSampleTimestamp-lastTimestamp_;
        
        LogTraceC << ". prev delay hard " << prevDelay 
            << " offset " << prevDelay - lastDelay_ << std::endl;

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
