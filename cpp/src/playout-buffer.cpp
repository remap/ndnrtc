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

//********************************************************************************
//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
PlayoutBuffer::PlayoutBuffer() :
frameBuffer_(nullptr),
providerThread_(*ThreadWrapper::CreateThread(frameProviderThreadRoutine, this)),
playoutCs_(*CriticalSectionWrapper::CreateCriticalSection()),
playoutFrames_(FrameBuffer::Slot::SlotComparator(true)),
framePointer_(0)
{
}
PlayoutBuffer::~PlayoutBuffer()
{
    providerThread_.SetNotAlive();
    providerThread_.Stop();
}

//********************************************************************************
#pragma mark - public
int PlayoutBuffer::init(ndnrtc::FrameBuffer *buffer)
{
    frameBuffer_ = buffer;

    if (buffer)
    {
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

shared_ptr<EncodedImage> PlayoutBuffer::acquireNextFrame(bool incCounter)
{
    shared_ptr<EncodedImage> frame(nullptr);
    FrameBuffer::Slot *slot = acquireNextSlot(incCounter);
    
    if (slot)
    {
        frame = slot->getFrame();
        framePointer_ = slot->getFrameNumber();
    }
    
    return frame;
}
FrameBuffer::Slot* PlayoutBuffer::acquireNextSlot(bool incCounter)
{
    FrameBuffer::Slot *slot = nullptr;
    
    if (!frameBuffer_)
    {
        NDNERROR("playout buffer was not initialized");
        return slot;
    }
    
    playoutCs_.Enter();
    if (playoutFrames_.size())
    {
        slot = playoutFrames_.top();
        
        if (framePointer_ > slot->getFrameNumber())
            TRACE(">>>>>LATE FRAMES");
        
        framePointer_ = slot->getFrameNumber();
        
        TRACE("frame %d acquired for playback", framePointer_);
    }
    else
    {
//        TRACE("no frames for playout");
        if (incCounter)
            framePointer_++;
    }
    playoutCs_.Leave();
    
    return slot;
}

void PlayoutBuffer::releaseAcquiredFrame()
{
    if (playoutFrames_.size())
    {
        playoutCs_.Enter();
        FrameBuffer::Slot *slot = playoutFrames_.top();
        playoutFrames_.pop();
        playoutCs_.Leave();
        
        frameBuffer_->unlockSlot(slot->getFrameNumber());
        frameBuffer_->markSlotFree(slot->getFrameNumber());
        framePointer_++;
    }
}

//********************************************************************************
#pragma mark - private
bool PlayoutBuffer::processFrameProvider()
{
    int eventsMask = FrameBuffer::Event::EventTypeReady;
    FrameBuffer::Event ev = frameBuffer_->waitForEvents(eventsMask);
    
    switch (ev.type_) {
        case FrameBuffer::Event::EventTypeReady:
        {
            TRACE("slot %d is ready", ev.frameNo_);
            playoutCs_.Enter();
            playoutFrames_.push(ev.slot_);
            frameBuffer_->lockSlot(ev.frameNo_);
            playoutCs_.Leave();
//            TRACE("pushed new frame %d", ev.frameNo_);
        }
            break;
        default: // error
            TRACE("got event %d. returning false", ev.type_);
            return false;
    }
    
    return true;
}
