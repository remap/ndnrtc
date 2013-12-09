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
}
PlayoutBuffer::~PlayoutBuffer()
{
    providerThread_.SetNotAlive();
    providerThread_.Stop();
}

//******************************************************************************
#pragma mark - public
int PlayoutBuffer::init(ndnrtc::FrameBuffer *buffer,
                        unsigned int gop,
                        unsigned int jitterSize)
{
    gopSize_ = gop;
    minJitterSize_ = jitterSize;
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
    while (jitterBuffer_.size())
    {
        FrameBuffer::Slot *slot = jitterBuffer_.top();
        frameBuffer_->unlockSlot(slot->getFrameNumber());
        jitterBuffer_.pop();
    }
    
    switchToState(StateClear);
}

shared_ptr<EncodedImage> PlayoutBuffer::acquireNextFrame(bool incCounter)
{
    shared_ptr<EncodedImage> frame(nullptr);
    FrameBuffer::Slot *slot = acquireNextSlot(incCounter);
    
    if (slot)
        frame = slot->getFrame();
    
    return frame;
}

FrameBuffer::Slot* PlayoutBuffer::acquireNextSlot(bool incCounter)
{
    bool needFlush = false;
    FrameBuffer::Slot *slot = nullptr;
    
    if (!frameBuffer_)
    {
        NDNERROR("[PLAYOUT] ACQUIRE - buffer was not initialized");
        return slot;
    }
    
    playoutCs_.Enter();
    
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
            if (!jitterBuffer_.size())
            {
                needFlush = true;
                DBG("[PLAYOUT] ACQUIRE - moving forward");
            }
            else
            {
                if (playheadPointer_ == framePointer_)
                {
                    slot = jitterBuffer_.top();
                    DBG("[PLAYOUT] ACQUIRE - playhead %d, top %d (diff: %d), jitter %d",
                          playheadPointer_, framePointer_, framePointer_-playheadPointer_,
                          jitterBuffer_.size());
                }
                else
                    DBG("[PLAYOUT] ACQUIRE - MISSING frame %d", playheadPointer_);
            }
        }
            break;
        default:
            DBG("[PLAYOUT] ACQUIRE - unknown state");
            break;
    }
    
    playoutCs_.Leave();
    
    if (needFlush && callback_)
        callback_->onMissedFrame(playheadPointer_);
    
    return slot;
}

void PlayoutBuffer::releaseAcquiredFrame()
{
    if (state_ != StatePlayback)
        return;
    
    bool moveHead = false;
    
    if (playheadPointer_ >= framePointer_ &&
        jitterBuffer_.size())
    {
        webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
        FrameBuffer::Slot *slot = jitterBuffer_.top();
        
        if (slot->isKeyFrame())
            nKeyFramesInJitter_--;
        
        jitterBuffer_.pop();
        frameBuffer_->unlockSlot(slot->getFrameNumber());
        moveHead = true;
        
        framePointer_ = (jitterBuffer_.size())?jitterBuffer_.top()->getFrameNumber():framePointer_;
    }
    
    if (jitterBuffer_.size() || moveHead)
    {
        webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
        playheadPointer_++;
        
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
                
                if (ev.slot_->isKeyFrame())
                {
                    lastKeyFrameNo_ = ev.frameNo_;
                    nKeyFramesInJitter_++;
                }
                
                jitterBuffer_.push(ev.slot_);
            }

            frameBuffer_->lockSlot(ev.frameNo_);
            framePointer_ = jitterBuffer_.top()->getFrameNumber();
            
            if (callback_)
                callback_->onFrameAddedToJitter(ev.slot_);
            
            switch (state_) {
                case StateClear: // switch to buffering
                    switchToState(StateBuffering);
                    break;
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
            playoutCs_.Enter();
            
            jitterBuffer_ = FrameJitterBuffer(FrameBuffer::Slot::SlotComparator(true));
            lastKeyFrameNo_ = -1;
            nKeyFramesInJitter_ = 0;
            isPlaybackStarted_ = false;
            framePointer_ = 0;
            playheadPointer_ = 0;
            
            playoutCs_.Leave();
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
