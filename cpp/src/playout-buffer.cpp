//
//  playout-buffer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev

//#undef NDN_LOGGING

#include "playout-buffer.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
PlayoutBuffer::PlayoutBuffer() :
frameBuffer_(nullptr),
providerThread_(*ThreadWrapper::CreateThread(frameProviderThreadRoutine, this)),
playoutCs_(*CriticalSectionWrapper::CreateCriticalSection()),
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
jitterBuffer_(FrameBuffer::Slot::SlotComparator(true)),
framePointer_(0)
{
    resetData();
}
PlayoutBuffer::~PlayoutBuffer()
{
    providerThread_.SetNotAlive();
    providerThread_.Stop();
}

//******************************************************************************
#pragma mark - public
int PlayoutBuffer::init(FrameBuffer *buffer, double startPacketRate,
                        unsigned int minJitterSize)
{
    currentPlayoutTimeMs_ = 1000./startPacketRate;
    minJitterSize_ = minJitterSize;
    frameBuffer_ = buffer;
    
    if (buffer)
    {
        switchToState(StateClear);
        
        unsigned int tid = PROVIDER_THREAD_ID;
        providerThread_.Start(tid);
    }
    else
    {
        NDNERROR("can't initialize playout buffer without frame buffer");
        return -1;
    }
    
    return 0;
}

void PlayoutBuffer::flush()
{
    TRACE("flushing jitter buffer");
    
    while (jitterBuffer_.size())
    {
        FrameBuffer::Slot *slot = jitterBuffer_.top();
        frameBuffer_->unlockSlot(slot->getFrameNumber());
        jitterBuffer_.pop();
    }
    
    switchToState(StateClear);
}

FrameBuffer::Slot* PlayoutBuffer::acquireNextSlot(bool incCounter)
{
    FrameBuffer::Slot *slot = nullptr;
    
    if (!frameBuffer_)
    {
        NDNERROR("[PLAYOUT] ACQUIRE - buffer was not initialized");
        return slot;
    }
    
    playoutCs_.Enter();
    bufferUnderrun_ = false;
    missingFrame_ = false;
    
    switch (state_) {
        case StateBuffering: // do nothing
            DBG("[PLAYOUT] ACQUIRE - buffering... jitter size - %d",
                jitterBuffer_.size());
            break;
        case StateClear: // do nothing
            DBG("[PLAYOUT] ACQUIRE - not playing back");
            break;
        case StatePlayback:
        {
            bufferUnderrun_ = jitterBuffer_.size() == 0;
            missingFrame_ = (playheadPointer_ != framePointer_) && !bufferUnderrun_;
            
            if (!bufferUnderrun_ && !missingFrame_)
            {
                slot = jitterBuffer_.top();
                DBG("[PLAYOUT] ACQUIRE - playhead %d, top %d (diff: %d), jitter %d",
                    playheadPointer_,
                    framePointer_,
                    framePointer_-playheadPointer_,
                    jitterBuffer_.size());
            }
        }
            break;
        default:
            DBG("[PLAYOUT] ACQUIRE - unknown state");
            break;
    }
    
    adjustPlayoutTiming(slot);
    playoutCs_.Leave();    
    
    if (missingFrame_ && callback_)
        callback_->onMissedFrame(playheadPointer_);
    
    if (bufferUnderrun_ && callback_)
        callback_->onJitterBufferUnderrun();
    
    return slot;
}

int PlayoutBuffer::releaseAcquiredSlot()
{
    if (state_ != StatePlayback)
        return -1;
    
    bool moveHead = false;
    
    if (playheadPointer_ >= framePointer_ &&
        jitterBuffer_.size())
    {
        webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
        FrameBuffer::Slot *slot = jitterBuffer_.top();
        
        //        if (slot->isKeyFrame())
        //            nKeyFramesInJitter_--;
        
        jitterBuffer_.pop();
        frameBuffer_->unlockSlot(slot->getFrameNumber());
        moveHead = true;
        
        framePointer_ = (jitterBuffer_.size())?jitterBuffer_.top()->getFrameNumber():framePointer_;
    }
    
    if (jitterBuffer_.size() || moveHead)
    {
        webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
        
        frameBuffer_->markSlotFree(playheadPointer_++);
        
        if (this->callback_)
            this->callback_->onPlayheadMoved(playheadPointer_);
        
        DBG("[PLAYOUT] MOVE - playhead: %d, top: %d",
            playheadPointer_, framePointer_);
        
    }
    else
    {
        DBG("[PLAYOUT] bufferring??? waiting for %d frame", playheadPointer_);
        //      switchToState(PlayoutBuffer::StateBuffering);
    }
    
    return calculatePlayoutTime();
}

//******************************************************************************
#pragma mark - private
bool PlayoutBuffer::processFrameProvider()
{
    int eventsMask = FrameBuffer::Event::EventTypeReady;
    FrameBuffer::Event ev = frameBuffer_->waitForEvents(eventsMask);
    
    switch (ev.type_) {
        case FrameBuffer::Event::EventTypeReady:
        {
            {
                CriticalSectionScoped scopedCs(&playoutCs_);
                jitterBuffer_.push(ev.slot_);
                
                if (ev.slot_->isKeyFrame())
                {
                    nKeyFrames_++;
                    keyFrameAvgSize_ += ev.slot_->totalSegmentsNumber();
                    keyFrameAvgSize_ /= (double)nKeyFrames_;
                }
                else
                {
                    nDeltaFrames_++;
                    deltaFrameAvgSize_ += ev.slot_->totalSegmentsNumber();
                    deltaFrameAvgSize_ /= (double)nDeltaFrames_;
                }
            }
            
            frameBuffer_->lockSlot(ev.frameNo_);
            framePointer_ = jitterBuffer_.top()->getFrameNumber();
            
            if (callback_)
                callback_->onFrameAddedToJitter(ev.slot_);
            
            switch (state_) {
                case StateClear: // switch to buffering
                    switchToState(StateBuffering);
                // fall through - need to check whether jitter size is sufficient
                case StateBuffering:
                {
                    CriticalSectionScoped scopedCs(&syncCs_);
                    
                    if (jitterBuffer_.size() >= minJitterSize_)
                        switchToState(StatePlayback);
                }
                    break;
                case StatePlayback: // do nothing
                    break;
                default:
                    break;
            }
            
            DBG("[PLAYOUT] ADDED frame %d to jitter (size %d)", ev.frameNo_,
                jitterBuffer_.size());
        }
            break;
        default: // error
            TRACE("got event %d. returning false", ev.type_);
            return false;
    }
    
    return true;
}

void PlayoutBuffer::switchToState(State state)
{
    bool change = true;
    
    switch (state) {
        case StateUnknown:
            change = false;
            NDNERROR("[PLAYOUT] illegal switch to state UNKNOWN");
            break;
        case StateClear:
        {
            DBG("[PLAYOUT] switched to state CLEAR");
            resetData();
        }
            break;
        case StateBuffering:
        {
            DBG("[PLAYOUT] switched to state BUFFERING");
        }
            break;
        case StatePlayback:
        {
            playheadPointer_ = jitterBuffer_.top()->getFrameNumber();
            DBG("[PLAYOUT] switched to state PLAYBACK. playback will start "
                "from %d", playheadPointer_);
        }
            break;
        default:
        {
            DBG("[PLAYOUT] trying to switch to unknown state. aborting");
            change = false;
        }
            break;
    }
    
    if (change)
    {
        state_ = state;
        
        if (callback_)
            callback_->onBufferStateChanged(state_);
    }
}

void PlayoutBuffer::adjustPlayoutTiming(FrameBuffer::Slot *slot)
{
    bool frameWasNotPresent = !nextFramePresent_;
    
    // frameWasNotPresent - if on the last iteration frame was not in the buffer
    // but now it has been arrived. this basically means, that playout time for
    // the previous frame could not be calculated and it was played out for the
    // same time as the frame before. therefore, for the newly arrived frame,
    // it's timestamp should reflect this and accomodate previous frame's
    // playout time
    
    if (missingFrame_ || bufferUnderrun_ || frameWasNotPresent)
        lastFrameTimestampMs_ += adaptedPlayoutTimeMs_;
    else
        if (slot)
        {
            int64_t packetTimestamp = slot->getPacketTimestamp();
            
            if (lastFrameTimestampMs_ < packetTimestamp)
                lastFrameTimestampMs_ =  packetTimestamp;
            else
                TRACE("[PLAYOUT] frame was missing");
        }
}

int PlayoutBuffer::calculatePlayoutTime()
{
    if (jitterBuffer_.size())
    {
        FrameBuffer::Slot *nextSlot = jitterBuffer_.top();
        
        // check if it is a consecutive frame (playheadPointer_ was incremented
        // by releaseAcquiredSlot() call)
        // if not - next frame has not arrived yet - use previous playout time
        if ((nextFramePresent_ = (nextSlot->getFrameNumber() ==
                                  playheadPointer_)))
        {
            uint64_t nextSlotTimestamp = nextSlot->getPacketTimestamp();
            TRACE("[PLAYOUT] next frame %d, %ld",
                  nextSlot->getFrameNumber(), nextSlotTimestamp);
            
            currentPlayoutTimeMs_ = (int)((int64_t)nextSlotTimestamp-
                                          (int64_t)lastFrameTimestampMs_);
            
            TRACE("[PLAYOUT] got playout for %d - %d",
                  playheadPointer_-1, currentPlayoutTimeMs_);
            
            if (currentPlayoutTimeMs_ < 0)
                currentPlayoutTimeMs_ = 0;
        }
        else
            TRACE("[PLAYOUT] next frame is missing. infer playout time %d",
                  adaptedPlayoutTimeMs_);
    }
    else
        TRACE("[PLAYOUT] no further frames to calculate playout, infer: %d", adaptedPlayoutTimeMs_);
    
    // adjust playout time
    adaptedPlayoutTimeMs_ = getAdaptedPlayoutTime(currentPlayoutTimeMs_,
                                                  jitterBuffer_.size());
    TRACE("[PLAYOUT] adapted time %d, extra %d",
          adaptedPlayoutTimeMs_, ampExtraTimeMs_);
    assert(adaptedPlayoutTimeMs_>=0);
    return adaptedPlayoutTimeMs_;
}

int PlayoutBuffer::getAdaptedPlayoutTime(int playoutTimeMs, int jitterSize)
{
    // do not run AMP if the frame supposed to be skipped
    if (playoutTimeMs == 0)
        return 0;

    double thresholdSize = (double)ampThreshold_*(double)minJitterSize_;
    bool canConsumeExtraTime = jitterSize > thresholdSize;
    
#ifdef USE_AMP
    if (jitterSize <= thresholdSize)
    {
        canConsumeExtraTime = false;
        
        TRACE("[PLAYOUT-AMP] jitter hit AMP threshold - %d (%.2f)",
              jitterSize, thresholdSize);
        double jitterSizeDecrease = (double)(thresholdSize-jitterSize)/(double)thresholdSize;
        double playouTimeIncrease = jitterSizeDecrease;
        int playoutIncreaseMs = (int)ceil((double)playoutTimeMs*playouTimeIncrease);
        
        ampExtraTimeMs_ += playoutIncreaseMs;
        playoutTimeMs += playoutIncreaseMs;
        
        DBG("[PLAYOUT-AMP] increased playout time for %d: %d (by %.2f%%)",
            playheadPointer_-1, playoutTimeMs, playouTimeIncrease*100);
    }
#endif
    
    
#ifdef USE_AMP_V2
    // main idea:
    // if next frame is NOT present in playout buffer, AND present in
    // assembling buffer AND number of assembled segments is bigger than 50% of
    // total segments number (i.e. more than a half of the frame was already
    // received) THEN increase current frame playout time in order to give some
    // time for the next frame to be assembled
    
    if (!nextFramePresent_)
    {
        canConsumeExtraTime = false;
        
        if (frameBuffer_->getState(playheadPointer_) == FrameBuffer::Slot::StateAssembling)
        {
            shared_ptr<FrameBuffer::Slot> slot = frameBuffer_->getSlot(playheadPointer_);
            
            if (slot.get())
            {
                bool isKeyFrame = slot->isKeyFrame();
                int nAssembled = slot->assembledSegmentsNumber();
                int nTotal = slot->totalSegmentsNumber();
                
                double assembledLevel = (double)nAssembled/(double)nTotal;
                int playoutIncrease = 0;
                
                if (assembledLevel >= AmpMinimalAssembledLevel ||
                    isKeyFrame)
                {
                    // increase current frame playout time
                    playoutIncrease = (isKeyFrame)?(double)playoutTimeMs*1. : playoutTimeMs*.5;
                    playoutTimeMs += playoutIncrease;
                    ampExtraTimeMs_ += playoutIncrease;
                }
                
                TRACE("[PLAYOUT-AMP] next frame is key: %s (%d/%d - %.2f). "
                      "playout time of %d increased by %d (total %d)",
                      (isKeyFrame)?"YES":"NO", nAssembled, nTotal,
                      assembledLevel, playheadPointer_-1, playoutIncrease,
                      playoutTimeMs);
            }
            else
                // should not happen
                assert(false);
        } // if assembling
    } // if (!nextFramePresent_)
#endif
    
    if (canConsumeExtraTime && ampExtraTimeMs_)
    {
        // get the frame to-be played out
        shared_ptr<FrameBuffer::Slot> slot = frameBuffer_->getSlot(playheadPointer_-1);
        
        if (slot.get())
        {
            bool isKeyFrame = slot->isKeyFrame();
            int nTotal = slot->totalSegmentsNumber();
            
            int maxDecreasePerFrame = (int)ceil((double)playoutTimeMs*ExtraTimePerFrame);
            
            // now, if the frame is insignificant, we can skip it
            if (nTotal <= deltaFrameAvgSize_)
            {
                TRACE("[PLAYOUT-AMP] skipping insignificant frame with size %d "
                      "(%.2f avg)", deltaFrameAvgSize_);
                maxDecreasePerFrame = playoutTimeMs;
            }
            
            if (ampExtraTimeMs_ > maxDecreasePerFrame)
            {
                playoutTimeMs -= maxDecreasePerFrame;
                ampExtraTimeMs_ -= maxDecreasePerFrame;
                TRACE("[PLAYOUT-AMP] decreased playout by %d (%d left)",
                      maxDecreasePerFrame, ampExtraTimeMs_);
            }
            else
            {
                TRACE("[PLAYOUT-AMP] consumed extra time. decreased by %d",
                      ampExtraTimeMs_);
                
                playoutTimeMs -= ampExtraTimeMs_;
                ampExtraTimeMs_ = 0;
            }
        } // if slot
    }
    
    return playoutTimeMs;
}

void PlayoutBuffer::resetData()
{
    CriticalSectionScoped scopedCs(&playoutCs_);

    jitterBuffer_ = FrameJitterBuffer(FrameBuffer::Slot::SlotComparator(true));
    framePointer_ = 0;
    playheadPointer_ = 0;
    jitterSizeMs_ = 0;
    currentPlayoutTimeMs_ = 0;
    ampExtraTimeMs_ = 0;
    lastFrameTimestampMs_ = 0;
    currentPlayoutTimeMs_ = 0;
    adaptedPlayoutTimeMs_ = 0;
    minJitterSize_ = INT_MAX;
    minJitterSizeMs_ = INT_MAX;
    jitterSizeMs_ = 0;
    nextFramePresent_ = true;
    bufferUnderrun_ = false;
    missingFrame_ = false;
    nKeyFrames_ = 0;
    nDeltaFrames_ = 0;
    deltaFrameAvgSize_ = 0.;
    keyFrameAvgSize_ = 0.;
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
JitterTiming::JitterTiming():
playoutTimer_(*EventWrapper::Create())
{
    resetData();
}

//******************************************************************************
#pragma mark - public
void JitterTiming::flush()
{
    resetData();
    //    stop();
}
void JitterTiming::stop()
{
    playoutTimer_.StopTimer();
    playoutTimer_.Set();
}

int64_t JitterTiming::startFramePlayout()
{
    int64_t processingStart = NdnRtcUtils::microsecondTimestamp();
    TRACE("PROCESSING START: %ld", processingStart);
    
    if (playoutTimestampUsec_ == 0)
    {
        TRACE("[PLAYOUT] init jitter timing");
        playoutTimestampUsec_ = NdnRtcUtils::microsecondTimestamp();
    }
    else
    { // calculate processing delay from the previous iteration
        int64_t prevIterationProcTimeUsec = processingStart -
        playoutTimestampUsec_;
        
        TRACE("[PLAYOUT] prev iteration time %ld", prevIterationProcTimeUsec);
        
        // substract frame playout delay
        if (prevIterationProcTimeUsec >= framePlayoutTimeMs_*1000)
            prevIterationProcTimeUsec -= framePlayoutTimeMs_*1000;
        else
            // should not occur!
            assert(0);
        
        TRACE("[PLAYOUT] prev iter proc time %ld", prevIterationProcTimeUsec);
        
        // add this time to the average processing time
        processingTimeUsec_ += prevIterationProcTimeUsec;
        TRACE("[PLAYOUT] proc time %ld", prevIterationProcTimeUsec);
        
        playoutTimestampUsec_ = processingStart;
    }
    
    return playoutTimestampUsec_;
}

void JitterTiming::updatePlayoutTime(int framePlayoutTime)
{
    TRACE("producer playout time %ld", framePlayoutTime);
    
    int playoutTimeUsec = framePlayoutTime*1000;
    if (playoutTimeUsec < 0) playoutTimeUsec = 0;
    
    if (processingTimeUsec_ >= 1000)
    {
        TRACE("[PLAYOUT] accomodate processing time %ld",
              processingTimeUsec_);
        
        int processingUsec = (processingTimeUsec_/1000)*1000;
        
        TRACE("[PLAYOUT] processing %d", processingUsec);
        
        if (processingUsec > playoutTimeUsec)
        {
            TRACE("[PLAYOUT] skipping frame. processing %d, playout %d",
                  processingUsec, playoutTimeUsec);
            
            processingUsec = playoutTimeUsec;
            playoutTimeUsec = 0;
        }
        else
            playoutTimeUsec -= processingUsec;
        
        processingTimeUsec_ = processingTimeUsec_ - processingUsec;
        TRACE("[PLAYOUT] playout usec %d, processing %ld",
              playoutTimeUsec, processingTimeUsec_);
    }
    
    framePlayoutTimeMs_ = playoutTimeUsec/1000;
}

void JitterTiming::runPlayoutTimer()
{
    assert(framePlayoutTimeMs_ >= 0);
    if (framePlayoutTimeMs_ > 0)
    {
        TRACE("TIMER WAIT %d", framePlayoutTimeMs_);
        playoutTimer_.StartTimer(false, framePlayoutTimeMs_);
        playoutTimer_.Wait(WEBRTC_EVENT_INFINITE);
        TRACE("TIMER DONE");
    }
    else
        TRACE("playout time zero - skipping frame");
}

//******************************************************************************
void JitterTiming::resetData()
{
    framePlayoutTimeMs_ = 0;
    processingTimeUsec_ = 0;
    playoutTimestampUsec_ = 0;
}
