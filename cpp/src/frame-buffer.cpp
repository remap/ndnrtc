//
//  frame-buffer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING
//#undef DEBUG

#include "frame-buffer.h"
#include "ndnrtc-utils.h"
#include "video-receiver.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnFrameData::NdnFrameData(EncodedImage &frame)
{
    unsigned int headerSize_ = sizeof(FrameDataHeader);
    
    length_ = frame._length+headerSize_;
    data_ = (unsigned char*)malloc(length_);
    
    // copy frame data with offset of header
    memcpy(data_+headerSize_, frame._buffer, frame._length);
    
    // setup header
    ((FrameDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
    ((FrameDataHeader*)(&data_[0]))->encodedWidth_ = frame._encodedWidth;
    ((FrameDataHeader*)(&data_[0]))->encodedHeight_ = frame._encodedHeight;
    ((FrameDataHeader*)(&data_[0]))->timeStamp_ = frame._timeStamp;
    ((FrameDataHeader*)(&data_[0]))->capture_time_ms_ = frame.capture_time_ms_;
    ((FrameDataHeader*)(&data_[0]))->frameType_ = frame._frameType;
    ((FrameDataHeader*)(&data_[0]))->completeFrame_ = frame._completeFrame;
    ((FrameDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
}

//******************************************************************************
#pragma mark - public
int NdnFrameData::unpackFrame(unsigned int length_, const unsigned char *data,
                              webrtc::EncodedImage **frame)
{
    unsigned int headerSize_ = sizeof(FrameDataHeader);
    FrameDataHeader header = *((FrameDataHeader*)(&data[0]));
    
    // check markers
    if (header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR)
        return RESULT_ERR;
    
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, header.encodedWidth_,
                                          header.encodedHeight_);
    
    *frame = new webrtc::EncodedImage(const_cast<uint8_t*>(&data[headerSize_]),
                                      length_-headerSize_, size);
    (*frame)->_encodedWidth = header.encodedWidth_;
    (*frame)->_encodedHeight = header.encodedHeight_;
    (*frame)->_timeStamp = header.timeStamp_;
    (*frame)->capture_time_ms_ = header.capture_time_ms_;
    (*frame)->_frameType = header.frameType_;
    (*frame)->_completeFrame = header.completeFrame_;
    
    return RESULT_OK;
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnAudioData::NdnAudioData(AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    
    length_ = packet.length_+headerSize;
    data_ = (unsigned char*)malloc(length_);
    
    memcpy(data_+headerSize, packet.data_, packet.length_);
    
    ((AudioDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
    ((AudioDataHeader*)(&data_[0]))->isRTCP_ = packet.isRTCP_;
    ((AudioDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
}

//******************************************************************************
#pragma mark - public
int NdnAudioData::unpackAudio(unsigned int len, const unsigned char *data,
                              AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&data[0]));
    
    if (header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR)
        return RESULT_ERR;
    
    packet.isRTCP_ = header.isRTCP_;
    packet.length_ = len-headerSize;
    packet.data_ = (unsigned char*)data+headerSize;
    
    return RESULT_OK;
}

//******************************************************************************
// FrameBuffer::Slot
//******************************************************************************
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
    data_ = new unsigned char[slotSize];
}

FrameBuffer::Slot::~Slot()
{
    delete data_;
}

//******************************************************************************
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

//******************************************************************************
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

NdnAudioData::AudioPacket FrameBuffer::Slot::getAudioFrame()
{
    NdnAudioData::AudioPacket packet = {false, 0, 0};
        
    if ((state_ == StateReady ||
         (state_ == StateLocked && stashedState_ == StateReady)))
    {
        if (RESULT_FAIL(NdnAudioData::unpackAudio(assembledDataSize_, data_,
                                                  packet)))
            WARN("error unpacking audio");
    }
    else
        ERR("frame is not ready. state: %d", state_);
    
    return packet;
}


FrameBuffer::Slot::State FrameBuffer::Slot::appendSegment(unsigned int segmentNo,
                                                          unsigned int dataLength,
                                                          const unsigned char *data)
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

//******************************************************************************
// FrameBuffer
//******************************************************************************
const int FrameBuffer::Event::AllEventsMask =   FrameBuffer::Event::EventTypeReady|
FrameBuffer::Event::EventTypeFirstSegment|
FrameBuffer::Event::EventTypeFreeSlot|
FrameBuffer::Event::EventTypeTimeout|
FrameBuffer::Event::EventTypeError;

#pragma mark - construction/destruction
FrameBuffer::FrameBuffer():
bufferSize_(0),
slotSize_(0),
nTimeouts_(0),
bufferEvent_(*EventWrapper::Create()),
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
bufferEventsRWLock_(*RWLockWrapper::CreateRWLock())
{
    
}

FrameBuffer::~FrameBuffer()
{
    
}

//******************************************************************************
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
    
    updateStat(Slot::StateFree, bufferSize_);
    
    return 0;
}

void FrameBuffer::flush()
{
//    bufferEvent_.Set();
//    bufferEvent_.Reset();
    
    syncCs_.Enter();
    for (map<unsigned int, shared_ptr<Slot>>::iterator it = frameSlotMapping_.begin(); it != frameSlotMapping_.end(); ++it)
    {
        shared_ptr<Slot> slot = it->second;
        if (slot->getState() != Slot::StateLocked)
        {
            updateStat(slot->getState(), -1);
            
            freeSlots_.push_back(slot);
            slot->markFree();
//            frameSlotMapping_.erase(it);
            notifyBufferEventOccurred(0, 0, Event::EventTypeFreeSlot, slot.get());
            
            updateStat(slot->getState(), 1);
        }
    }
    fullTimeoutCase_ = 0;
    timeoutSegments_.clear();
    frameSlotMapping_.clear();
    syncCs_.Leave();
}

void FrameBuffer::release()
{
    forcedRelease_ = true;
    bufferEvent_.Set();
}

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
    
    updateStat(Slot::StateFree, -1);
    updateStat(freeSlot->getState(), 1);
    
    return CallResultNew;
}

void FrameBuffer::markSlotFree(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk &&
        slot->getState() != Slot::StateLocked)
    {
        updateStat(slot->getState(), -1);
        syncCs_.Enter();
        
        slot->markFree();
        freeSlots_.push_back(slot);
        frameSlotMapping_.erase(frameNumber);
        
        syncCs_.Leave();
        updateStat(slot->getState(), 1);
        
        notifyBufferEventOccurred(frameNumber, 0, Event::EventTypeFreeSlot, slot.get());
    }
    else
    {
        WARN("can't free slot %d - it was not found or locked", frameNumber);
    }
}

void FrameBuffer::lockSlot(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
    {
        updateStat(slot->getState(), -1);
        slot->markLocked();
        updateStat(slot->getState(), 1);
    }
    else
    {
        WARN("can't lock slot - it was not found");
    }
}

void FrameBuffer::unlockSlot(unsigned int frameNumber)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
    {
        updateStat(slot->getState(), -1);
        slot->markUnlocked();
        updateStat(slot->getState(), 1);
    }
    else
    {
        WARN("can't unlock slot - it was not found");
    }
}

void FrameBuffer::markSlotAssembling(unsigned int frameNumber, unsigned int totalSegments, unsigned int segmentSize)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
    {
        updateStat(slot->getState(), -1);
        slot->markAssembling(totalSegments, segmentSize);
        updateStat(slot->getState(), 1);
    }
    else
    {
        WARN("can't unlock slot - it was not found");
    }
}

FrameBuffer::CallResult FrameBuffer::appendSegment(unsigned int frameNumber, unsigned int segmentNumber,
                                                   unsigned int dataLength, const unsigned char *data)
{
    shared_ptr<Slot> slot;
    CallResult res = CallResultError;
    
    if ((res = getFrameSlot(frameNumber, &slot)) == CallResultOk)
    {
        if (slot->getState() == Slot::StateAssembling)
        {
            uint64_t segmentId = (uint64_t)frameNumber*1000+segmentNumber;
            
            timeoutSegments_.erase(segmentId);
            fullTimeoutCase_ = 0;
            
            res = CallResultAssembling;
            updateStat(slot->getState(), -1);
            
            syncCs_.Enter();
            Slot::State slotState = slot->appendSegment(segmentNumber, dataLength, data);
            syncCs_.Leave();
            
            updateStat(slotState, 1);
            // if we got 1st segment - notify regardless of other events
            if (slot->assembledSegmentsNumber() == 1)
                notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeFirstSegment, slot.get());
            
            switch (slotState)
            {
                case Slot::StateAssembling:
                    break;
                case Slot::StateReady: // slot ready event
                    notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeReady, slot.get());
                    res = CallResultReady;
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
    {
        uint64_t segmentId = (uint64_t)frameNumber*1000+segmentNumber;
        
        if (timeoutSegments_.find(segmentId) == timeoutSegments_.end())
            timeoutSegments_[segmentId] = 0;

        timeoutSegments_[segmentId]++;
        nTimeouts_++;
        
        // if the number of timeouted slots equals buffer size - setup full
        // buffer checking
        if (abs((long)lastTimeoutFrameNo_-(long)frameNumber+1) == bufferSize_)
        {
            fullTimeoutCase_++;
            TRACE("full timeout case %d", fullTimeoutCase_);
        }
        
        TRACE("timeout frame %d", frameNumber);
        lastTimeoutFrameNo_ = frameNumber;
        
        // we encoutered full timeout event critical number of times -
        // generate full timeout event (by setting -1 to framenumber and
        // segment number)
        if (fullTimeoutCase_ >= FULL_TIMEOUT_CASE_THRESHOLD)
        {
            frameNumber = (unsigned int)-1;
            segmentNumber = (unsigned)-1;
            TRACE("full timeout %d %d", frameNumber, segmentNumber);
        }
        
        notifyBufferEventOccurred(frameNumber, segmentNumber, Event::EventTypeTimeout, slot.get());
    }
    else
    {
        WARN("can't notify slot %d - it was not found. current size: %d", frameNumber,
             frameSlotMapping_.size());
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

shared_ptr<EncodedImage> FrameBuffer::getEncodedImage(unsigned int frameNo)
{
    EncodedImage encodedImage;
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(frameNo, &slot) == CallResultOk &&
        slot->getState() == Slot::StateReady)
        return  slot->getFrame();
    
    return shared_ptr<EncodedImage>(nullptr);
}

void FrameBuffer::renameSlot(unsigned int oldFrameNo, unsigned int newFrameNo)
{
    EncodedImage encodedImage;
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(oldFrameNo, &slot, true) == CallResultOk)
    {
        slot->rename(newFrameNo);
        frameSlotMapping_[newFrameNo] = slot;
    }
    
    return;
}

FrameBuffer::Event FrameBuffer::waitForEvents(int &eventsMask, unsigned int timeout)
{
    unsigned int wbrtcTimeout = (timeout == 0xffffffff)?WEBRTC_EVENT_INFINITE:timeout;
    bool stop = false;
    Event poppedEvent;
    
    memset(&poppedEvent, 0, sizeof(poppedEvent));
    poppedEvent.type_ = Event::EventTypeError;
    forcedRelease_ = false;
    
    while (!(stop || forcedRelease_))
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

unsigned int FrameBuffer::getStat(Slot::State state)
{
    if (statistics_.find(state) == statistics_.end())
        statistics_[state] = 0;
    
    return statistics_[state];
}
//******************************************************************************
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

void FrameBuffer::updateStat(Slot::State state, int change)
{
    if (statistics_.find(state) == statistics_.end())
        statistics_[state] = 0;
    
    statistics_[state] += change;
}

