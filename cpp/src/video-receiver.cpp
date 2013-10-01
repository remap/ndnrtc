//
//  video-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/21/13
//

#include "video-receiver.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//********************************************************************************
//********************************************************************************
const string VideoReceiverParams::ParamNameProducerRate = "producer-rate";
const string VideoReceiverParams::ParamNameReceiverId = "receiver-id";

//********************************************************************************
// FrameBuffer::Slot
//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
FrameBuffer::Slot::Slot(unsigned int slotSize) :
dataLength_(slotSize),
state_(FrameBuffer::Slot::StateFree),
segmentSize_(0),
storedSegments_(0),
segmentsNum_(0),
assembledDataSize_(0),
frameNumber_(-1)
{
    TRACE("");
    data_ = new unsigned char[slotSize];
}

FrameBuffer::Slot::~Slot()
{
    TRACE("");
    delete data_;
}

//********************************************************************************
#pragma mark - all static
shared_ptr<string> FrameBuffer::Slot::stateToString(FrameBuffer::Slot::State state)
{
    switch (state) {
        case StateFree:
            return shared_ptr<string>(new string(STR(StateFree)));
        case StateAssembling:
            return shared_ptr<string>(new string(STR(StateAssembling)));
        case StateNew:
            return shared_ptr<string>(new string(STR(StateNew)));
        case StateReady:
            return shared_ptr<string>(new string(STR(StateReady)));
        case StateLocked:
        default:
            return shared_ptr<string>(new string(STR(StateLocked)));
    }
}

//********************************************************************************
#pragma mark - public
shared_ptr<EncodedImage> FrameBuffer::Slot::getFrame()
{
    EncodedImage *frame = nullptr;
    
    if ((state_ == StateReady ||
         (state_ == StateLocked && stashedState_ == StateReady)) &&
        NdnFrameData::unpackFrame(assembledDataSize_, data_, &frame) < 0)
        ERR("error unpacking frame");
    
    return shared_ptr<EncodedImage>(frame);
}

FrameBuffer::Slot::State FrameBuffer::Slot::appendSegment(unsigned int segmentNo, unsigned int dataLength, unsigned char *data)
{
    if (!(state_ == StateAssembling))
    {
        ERR("slot is not in a writeable state - %s", Slot::stateToString(state_)->c_str());
        return state_;
    }
    
    unsigned char *pos = (data_ + segmentNo * segmentSize_);
    
    memcpy(pos, data, dataLength);
    assembledDataSize_ += dataLength;
    storedSegments_++;
    
    // check if we've collected all segments
    if (storedSegments_ == segmentsNum_)
        state_ = StateReady;
    else
        state_ = StateAssembling;
    
    return state_;
}

//********************************************************************************
// FrameBuffer
//********************************************************************************
const int FrameBuffer::Event::AllEventsMask =   FrameBuffer::Event::EventTypeReady|
FrameBuffer::Event::EventTypeFirstSegment|
FrameBuffer::Event::EventTypeFreeSlot|
FrameBuffer::Event::EventTypeTimeout|
FrameBuffer::Event::EventTypeError;

#pragma mark - construction/destruction
FrameBuffer::FrameBuffer():
bufferSize_(0),
slotSize_(0),
bufferEvent_(*EventWrapper::Create()),
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
bufferEventsRWLock_(*RWLockWrapper::CreateRWLock())
{
    
}


FrameBuffer::~FrameBuffer()
{
    
}

//********************************************************************************
#pragma mark - public
int FrameBuffer::init(unsigned int bufferSize, unsigned int slotSize)
{
    if (!bufferSize || !slotSize)
    {
        ERR("bad arguments");
        return -1;
    }
    
    bufferSize_ = bufferSize;
    slotSize_ = slotSize;
    
    // create slots
    for (int i = 0; i < bufferSize_; i++)
    {
        shared_ptr<Slot> slot(new Slot(slotSize_));
        
        freeSlots_.push_back(slot);
        notifyBufferEventOccurred(0, 0, Event::EventTypeFreeSlot, slot.get());
    }
    
    return 0;
}
//int FrameBuffer::flush()
//{
//    bufferEvent_.Set();
//    bufferEvent_.Reset();
//    
//    syncCs_.Enter();
//    for (map<unsigned int, shared_ptr<Slot>>::iterator it = frameSlotMapping_.begin(); it != frameSlotMapping_.end(); ++it)
//    {
//        shared_ptr<Slot> slot = it->second;
//        if (slot->getState() != Slot::StateLocked)
//        {
//            freeSlots_.push_back(slot);
//            slot->markFree();
//            frameSlotMapping_.erase(it);
//        }
//    }
//    syncCs_.Leave();
//    
//    return 0;
//}

FrameBuffer::CallResult FrameBuffer::bookSlot(unsigned int frameNumber)
{
    shared_ptr<Slot> freeSlot;
    
    // check if this frame number is already booked
    if (getFrameSlot(frameNumber, &freeSlot) == CallResultOk)
        return CallResultBooked;
    
    if (!freeSlots_.size())
        return CallResultFull;
    
    syncCs_.Enter();
    
    freeSlot = freeSlots_.back();
    freeSlots_.pop_back();
    
    frameSlotMapping_[frameNumber] = freeSlot;
    freeSlot->markNew(frameNumber);
    
    syncCs_.Leave();
    
    return CallResultNew;
}

void FrameBuffer::markSlotFree(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk &&
        slot->getState() != Slot::StateLocked)
    {
        syncCs_.Enter();
        
        // remove slot from ready slots array
        //        if (slot->getState() == Slot::StateReady)
        //            readySlots_.erase(readySlots_.find(frameNumber));
        
        slot->markFree();
        freeSlots_.push_back(slot);
        frameSlotMapping_.erase(frameNumber);
        syncCs_.Leave();
        
        notifyBufferEventOccurred(frameNumber, 0, Event::EventTypeFreeSlot, slot.get());
    }
    else
    {
        WARN("can't free slot - it was not found or locked");
    }
}

void FrameBuffer::lockSlot(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
        slot->markLocked();
    else
    {
        WARN("can't lock slot - it was not found");
    }
}

void FrameBuffer::unlockSlot(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
        slot->markUnlocked();
    else
    {
        WARN("can't unlock slot - it was not found");
    }
}

void FrameBuffer::markSlotAssembling(unsigned int frameNumber, unsigned int totalSegments, unsigned int segmentSize)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
        slot->markAssembling(totalSegments, segmentSize);
    else
    {
        WARN("can't unlock slot - it was not found");
    }
}

FrameBuffer::CallResult FrameBuffer::appendSegment(unsigned int frameNumber, unsigned int segmentNumber,
                                                   unsigned int dataLength, unsigned char *data)
{
    shared_ptr<Slot> slot;
    CallResult res = CallResultError;
    
    if ((res = getFrameSlot(frameNumber, &slot)) == CallResultOk)
    {
        if (slot->getState() == Slot::StateAssembling)
        {
            res = CallResultAssembling;
            
            syncCs_.Enter();
            Slot::State slotState = slot->appendSegment(segmentNumber, dataLength, data);
            syncCs_.Leave();
            
            switch (slotState)
            {
                case Slot::StateAssembling:
                    if (slot->assembledSegmentsNumber() == 1) // first segment event
                        notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeFirstSegment, slot.get());
                    break;
                case Slot::StateReady: // slot ready event
                    //                    readySlots_[frameNumber] = slot; // save slot in ready slots array
                    notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeReady, slot.get());
                    break;
                default:
                    WARN("trying to append segment to non-writeable slot. slot state: %s", Slot::stateToString(slotState)->c_str());
                    res = (slotState == Slot::StateLocked)?CallResultLocked:CallResultError;
                    break;
            }
        }
        else
        {
            WARN("slot was booked but not marked assembling");
        }
    }
    else
    {
        WARN("trying to append segment to non-booked slot");
    }
    
    return res;
}

void FrameBuffer::notifySegmentTimeout(unsigned int frameNumber, unsigned int segmentNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
        notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeTimeout, slot.get());
    else
    {
        WARN("can't unlock slot - it was not found");
    }
}

FrameBuffer::Slot::State FrameBuffer::getState(unsigned int frameNo)
{
    Slot::State state;
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNo, &slot) == CallResultOk)
        return slot->getState();
    
    return Slot::StateFree;
}

FrameBuffer::Event FrameBuffer::waitForEvents(int &eventsMask, unsigned int timeout)
{
    unsigned int wbrtcTimeout = (timeout == 0xffffffff)?WEBRTC_EVENT_INFINITE:timeout;
    bool stop = false;
    Event poppedEvent;
    
    memset(&poppedEvent, 0, sizeof(poppedEvent));
    poppedEvent.type_ = Event::EventTypeError;
    
    while (!stop)
    {
        bufferEventsRWLock_.AcquireLockShared();
        
        list<Event>::iterator it = pendingEvents_.begin();
        
        // iterate through pending events
        while (!(stop || it == pendingEvents_.end()))
        {
            if ((*it).type_ & eventsMask) // questioned event type found in pending events
            {
                poppedEvent = *it;
                stop = true;
            }
            else
                it++;
        }
        
        bufferEventsRWLock_.ReleaseLockShared();
        
        if (stop)
        {
            bufferEventsRWLock_.AcquireLockExclusive();
            pendingEvents_.erase(it);
            bufferEventsRWLock_.ReleaseLockExclusive();
        }
        else
            // if couldn't find event we are looking for - wait for the event to occur
            stop = (bufferEvent_.Wait(wbrtcTimeout) != kEventSignaled);
    }
    
    return poppedEvent;
}

//********************************************************************************
#pragma mark - private
void FrameBuffer::notifyBufferEventOccurred(unsigned int frameNo, unsigned int segmentNo,
                                            Event::EventType eType, Slot *slot)
{
    Event ev;
    ev.type_ = eType;
    ev.segmentNo_ = segmentNo;
    ev.frameNo_ = frameNo;
    ev.slot_ = slot;
    
    bufferEventsRWLock_.AcquireLockExclusive();
    pendingEvents_.push_back(ev);
    bufferEventsRWLock_.ReleaseLockExclusive();
    
    // notify about event
    bufferEvent_.Set();
}

FrameBuffer::CallResult FrameBuffer::getFrameSlot(unsigned int frameNo, shared_ptr<Slot> *slot, bool remove)
{
    CallResult res = CallResultNotFound;
    map<unsigned int, shared_ptr<Slot>>::iterator it;
    
    syncCs_.Enter();
    it = frameSlotMapping_.find(frameNo);
    
    if (it != frameSlotMapping_.end())
    {
        *slot = it->second;
        res = CallResultOk;
        
        if (remove)
            frameSlotMapping_.erase(it);
    }
    syncCs_.Leave();
    
    return res;
}

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(NdnParams *params) : NdnRtcObject(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this)),
pipelineThread_(*ThreadWrapper::CreateThread(pipelineThreadRoutine, this)),
assemblingThread_(*ThreadWrapper::CreateThread(assemblingThreadRoutine, this)),
mode_(ReceiverModeCreated),
playout_(false),
playoutFrameNo_(0),
playoutSleepIntervalUSec_(33333),
face_(nullptr),
frameConsumer_(nullptr)
{
    
}

NdnVideoReceiver::~NdnVideoReceiver()
{
    stopFetching();
}

//********************************************************************************
#pragma mark - public
int NdnVideoReceiver::init(shared_ptr<Face> face)
{
    mode_ = ReceiverModeInit;
    return 0;
}

int NdnVideoReceiver::startFetching()
{
    if (mode_ != ReceiverModeInit)
        return notifyError(-1, "receiver is not initialized or already started");
    
    // setup and start playout thread
    unsigned int tid = getParams()->getReceiverId()<<1+1;
    playoutSleepIntervalUSec_ = 1000000/getParams()->getProducerRate();
    
    if (!playoutThread_.Start(tid))
        return notifyError(-1, "can't start playout thread");
    
    playout_ = true;
    
    // setup and start assembling thread
    
    // setup and start pipeline thread
    
    mode_ = ReceiverModeWaitingFirstSegment;
    
    return 0;
}

int NdnVideoReceiver::stopFetching()
{
    if (mode_ == ReceiverModeCreated || mode_ == ReceiverModeInit)
        return 0; // notifyError(-1, "fetching was not started");
    
    // stop playout thread
    playoutThread_.SetNotAlive();
    playout_ = false;
    playoutThread_.Stop();
    
    // stop assembling thread
    
    // stop pipeline thread
    
    mode_ = ReceiverModeInit;
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnVideoReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    TRACE("interest timeout: %s", interest.toUri().c_str());
}
void NdnVideoReceiver::onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
{
    TRACE("got data for interest: %s", interest.toUri().c_str());
}

//********************************************************************************
#pragma mark - private
bool NdnVideoReceiver::processPlayout()
{
#if 0
#warning while or if?
    while (playout_)
    {
        int64_t processingDelay = NdnRtcUtils::microsecondTimestamp();
        
        FrameBuffer::Slot *frameSlot = nullptr;
        
        FrameBuffer::CallResult res =  frameBuffer_.getFrame(playoutFrameNo_, frameSlot);
        
        if (res == FrameBuffer::CallResultOk ||
            res == FrameBuffer::CallResultOtherFrame)
        {
            playoutFrameNo_ = frameSlot->getFrameNumber();
            
            EncodedImage *frame = frameSlot->getFrame();
            
            if (frameConsumer_)
            {
                // lock slot until frame processing (decoding) is done
                frameBuffer_.lockSlot(playoutFrameNo_);
                frameConsumer_->onEncodedFrameDelivered(*frame);
                frameBuffer_.unlockSlot(playoutFrameNo_);
            }
        }
        else
        {
            WARN("couldn't get frame with number %d (cause: %d)", playoutFrameNo_, res);
        }
        
        // mark slot as free
        frameBuffer_.markFree(playoutFrameNo_);
        playoutFrameNo_++;
        
        processingDelay = NdnRtcUtils::microsecondTimestamp()-processingDelay;
        
        // sleep if needed
        if (playoutSleepIntervalUSec_-processingDelay)
            usleep(playoutSleepIntervalUSec_-processingDelay);
    }
#endif
    return true;
}

bool NdnVideoReceiver::processInterests()
{
    return true;
}

bool NdnVideoReceiver::processAssembling()
{
    return true;
}
