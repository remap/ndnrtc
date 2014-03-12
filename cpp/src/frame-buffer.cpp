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
// FrameBuffer::Slot
//******************************************************************************
#pragma mark - construction/destruction
FrameBuffer::Slot::Slot(unsigned int slotSize) :
dataLength_(slotSize),
state_(FrameBuffer::Slot::StateFree),
segmentSize_(0),
fetchedSegments_(0),
segmentsNum_(0),
assembledDataSize_(0),
bookingId_(-1),
cs_(*CriticalSectionWrapper::CreateCriticalSection())
{
    data_ = new unsigned char[slotSize];
    reset();
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
        LOG_NDNERROR("error unpacking frame");
    
    return shared_ptr<EncodedImage>(frame);
}

NdnAudioData::AudioPacket FrameBuffer::Slot::getAudioFrame()
{
    NdnAudioData::AudioPacket packet = {false, -1, 0, 0};
    
    if ((state_ == StateReady ||
         (state_ == StateLocked && stashedState_ == StateReady)))
    {
        if (RESULT_FAIL(NdnAudioData::unpackAudio(assembledDataSize_, data_,
                                                  packet)))
            LOG_WARN("error unpacking audio");
    }
    else
        LOG_NDNERROR("frame is not ready. state: %d", state_);
    
    return packet;
}

int FrameBuffer::Slot::getPacketMetadata(PacketData::PacketMetadata &metadata) const
{
    if (isMetaReady())
    {
        // check, whether it's a video or an audio packet - check 1st byte
        int16_t marker = *(int16_t*)data_;
        
        if (marker == (int16_t)NDNRTC_FRAMEHDR_MRKR) // video
            return NdnFrameData::unpackMetadata(assembledDataSize_, data_,
                                                metadata);
        else if (marker == (int16_t)NDNRTC_AUDIOHDR_MRKR) // audio
            return NdnAudioData::unpackMetadata(assembledDataSize_, data_,
                                                metadata);
    }
    
    // otherwise - error
    return RESULT_ERR;
}

int64_t FrameBuffer::Slot::getPacketTimestamp()
{
    if (isMetaReady())
    {
        // check, whether it's video or audio packet - check 1st byte
        if (NdnFrameData::isVideoData(segmentSize_, data_))
            return NdnFrameData::getTimestamp(segmentSize_, data_);
        
        if (NdnAudioData::isAudioData(segmentSize_, data_))
            return NdnAudioData::getTimestamp(segmentSize_, data_);
    }
    
    // otherwise - error
    return RESULT_ERR;
}

FrameBuffer::Slot::State
FrameBuffer::Slot::appendSegment(SegmentNumber segmentNo,
                                 unsigned int dataLength,
                                 const unsigned char *data)
{
    if (!(state_ == StateAssembling))
    {
        LOG_NDNERROR("slot is not in a writeable state - %s", Slot::stateToString(state_)->c_str());
        return state_;
    }
    
    unsigned char *pos = (data_ + segmentNo * segmentSize_);
    
    memcpy(pos, data, dataLength);
    assembledDataSize_ += dataLength;
    fetchedSegments_++;
    
    LOG_TRACE("append sno %d len %d to id %d state %d",
          segmentNo, dataLength, getBookingId(), state_);
    
    // check if we've collected all segments
    if (fetchedSegments_ == segmentsNum_)
    {
        stateTimestamps_[Slot::StateReady] = NdnRtcUtils::microsecondTimestamp();
        state_ = StateReady;
        LOG_TRACE("marked ready id %d frame %d", getBookingId(), getFrameNumber());
    }
    else
        state_ = StateAssembling;
    
    {
        webrtc::CriticalSectionScoped scopedCs(&cs_);
        missingSegments_.erase(segmentNo);
    }
    
    return state_;
}

void FrameBuffer::Slot::markAssembling(unsigned int segmentsNum,
                                       unsigned int segmentSize)
{
    LOG_TRACE("mark assembling id %d state %d snum %d segsz %d",
          getBookingId(), state_, segmentsNum, segmentSize);
    
    state_ = StateAssembling;
    segmentSize_ = segmentSize;
    segmentsNum_ = segmentsNum;
    stateTimestamps_[Slot::StateAssembling] = NdnRtcUtils::microsecondTimestamp();
    
    {
        webrtc::CriticalSectionScoped scopedCs(&cs_);
        missingSegments_.clear();
        
        for (int segNo = 0; segNo < segmentsNum; segNo++)
            missingSegments_.insert(segNo);
    }
    
    if (dataLength_ < segmentsNum_*segmentSize_)
    {
        LOG_WARN("slot size is smaller than expected amount of data. "
                 "enlarging buffer...");
        dataLength_ = 2*segmentsNum_*segmentSize_;
        data_ = (unsigned char*)realloc(data_, dataLength_);
    }
}

//******************************************************************************
#pragma mark - private
void FrameBuffer::Slot::reset()
{
    bookingId_ = -1;
    isKeyFrame_ = false;
    state_ = StateFree;
    assembledDataSize_ = 0;
    fetchedSegments_ = 0;
    segmentsNum_ = 0;
    nRetransmitRequested_ = 0;
    
    missingSegments_.clear();
    
    stateTimestamps_.clear();
    stateTimestamps_[state_] = NdnRtcUtils::microsecondTimestamp();
}

//******************************************************************************
// FrameBuffer
//******************************************************************************
const int FrameBuffer::Event::AllEventsMask =   FrameBuffer::Event::Ready|
FrameBuffer::Event::FirstSegment|
FrameBuffer::Event::FreeSlot|
FrameBuffer::Event::Timeout|
FrameBuffer::Event::Error;

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

//******************************************************************************
#pragma mark - public
int FrameBuffer::init(unsigned int bufferSize, unsigned int slotSize,
                      unsigned int segmentSize)
{
    if (!bufferSize || !slotSize)
    {
        NDNERROR("bad arguments");
        return -1;
    }
    
    bufferSize_ = bufferSize;
    slotSize_ = slotSize;
    segmentSize_ = segmentSize;
    forcedRelease_ = false;    
    
    // create slots
    for (int i = 0; i < bufferSize_; i++)
    {
        shared_ptr<Slot> slot(new Slot(slotSize_));
        
        freeSlots_.push_back(slot);
        notifyBufferEventOccurred(-1, -1, Event::FreeSlot, slot.get());
        TRACE("free slot - pending %d, free %d", pendingEvents_.size(),
              freeSlots_.size());
    }
    
    updateStat(Slot::StateFree, bufferSize_);
    
    return 0;
}

void FrameBuffer::flush()
{
    {
        CriticalSectionScoped scopedCs(&syncCs_);
        TRACE("flushing frame buffer - size %d, pending %d, free %d",
              activeSlots_.size(), pendingEvents_.size(),
              freeSlots_.size());
        
        for (map<Name, shared_ptr<Slot>>::iterator it = activeSlots_.begin();
             it != activeSlots_.end(); ++it)
        {
            shared_ptr<Slot> slot = it->second;
            if (slot->getState() != Slot::StateLocked)
            {
                updateStat(slot->getState(), -1);
                
                freeSlots_.push_back(slot);
                slot->markFree();
                notifyBufferEventOccurred(-1, -1, Event::FreeSlot, slot.get());
                
                updateStat(slot->getState(), 1);
            }
            else
                TRACE("trying to flush locked slot %d", slot->getFrameNumber());
        }
        activeSlots_.clear();
        resetData();
    }
    
    TRACE("flushed. pending events %d", pendingEvents_.size());
}

void FrameBuffer::release()
{
    TRACE("[BUFFER] RELEASE");
    forcedRelease_ = true;
    bufferEvent_.Set();
}

FrameBuffer::CallResult FrameBuffer::bookSlot(const Name& prefix,
                                              BookingId& bookingId)
{
    shared_ptr<Slot> freeSlot;
    Name bookingPrefix = prefix;
    NdnRtcNamespace::trimSegmentNumber(prefix, bookingPrefix);
    
    // check if this frame number is already booked
    if (getFrameSlot(bookingPrefix, &freeSlot) == CallResultOk)
        return CallResultBooked;
    
    if (!freeSlots_.size())
        return CallResultFull;
    
    syncCs_.Enter();
    
    freeSlot = freeSlots_.back();
    freeSlots_.pop_back();
    activeSlots_[bookingPrefix] = freeSlot;
    bookingId = getBookingIdForPrefix(bookingPrefix);
    freeSlot->markNew(bookingId, NdnRtcNamespace::isKeyFramePrefix(bookingPrefix));
    
    dumpActiveSlots();
    
    syncCs_.Leave();
    
    TRACE("booked %d - %s", bookingId, bookingPrefix.toUri().c_str());
    
    updateStat(Slot::StateFree, -1);
    updateStat(freeSlot->getState(), 1);
    
    return CallResultNew;
}

void FrameBuffer::markSlotFree(PacketNumber packetNo)
{
    shared_ptr<Slot> slot;
    Name prefix;
    
    if (getFrameSlot(packetNo, &slot, prefix) == CallResultOk &&
        slot->getState() != Slot::StateLocked)
    {
        markSlotFree(prefix, slot);
    }
    else
    {
        TRACE("can't free slot %d - it was not found or locked",
              packetNo);
    }
}

void FrameBuffer::markSlotFree(const Name &prefix)
{
    shared_ptr<Slot> slot;
    
    if (getFrameSlot(prefix, &slot) == CallResultOk &&
        slot->getState() != Slot::StateLocked)
    {
        markSlotFree(prefix, slot);
//        updateStat(slot->getState(), -1);
//        syncCs_.Enter();
//        
//        slot->markFree();
//        freeSlots_.push_back(slot);
//        activeBookings_.erase(slot->getBookingId());
//        TRACE("added free slot. size %d", freeSlots_.size());
//        syncCs_.Leave();
//        updateStat(slot->getState(), 1);
//        
//        notifyBufferEventOccurred(-1, -1, Event::FreeSlot, slot.get());
    }
    else
    {
        TRACE("can't free slot %s - it was not found or locked",
              prefix.to_uri().c_str());
    }
}

void FrameBuffer::lockSlot(PacketNumber frameNumber)
{
    shared_ptr<Slot> slot;
    Name prefix;
    
    if (getFrameSlot(frameNumber, &slot, prefix) == CallResultOk)
    {
        TRACE("locking slot %d (%d)", slot->getBookingId(),
              slot->getFrameNumber());
        updateStat(slot->getState(), -1);
        slot->markLocked();
        updateStat(slot->getState(), 1);
    }
    else
    {
        TRACE("can't lock slot - it was not found");
    }
}

void FrameBuffer::unlockSlot(PacketNumber frameNumber)
{
    shared_ptr<Slot> slot;
    Name prefix;
    
    if (getFrameSlot(frameNumber, &slot, prefix) == CallResultOk)
    {
        TRACE("unlocking %d (%d)", slot->getBookingId(), slot->getFrameNumber());
        updateStat(slot->getState(), -1);
        slot->markUnlocked();
        updateStat(slot->getState(), 1);
    }
    else
    {
        TRACE("can't unlock slot - it was not found");
    }
}

//void FrameBuffer::markSlotAssembling(PacketNumber frameNumber,
//                                     unsigned int totalSegments,
//                                     unsigned int segmentSize,
//                                     bool useKeyNamespace)
//{
//    shared_ptr<Slot> slot;
//    
//    if (getFrameSlot(frameNumber, &slot) == CallResultOk)
//    {
//        updateStat(slot->getState(), -1);
//        slot->markAssembling(totalSegments, segmentSize);
//        updateStat(slot->getState(), 1);
//    }
//    else
//    {
//        TRACE("can't unlock slot - it was not found");
//    }
//}

FrameBuffer::CallResult FrameBuffer::appendSegment(const Data &data)
{
    Name prefix = data.getName();
    
    shared_ptr<Slot> slot;
    CallResult res = CallResultError;
    
    if ((res = getFrameSlot(prefix, &slot)) != CallResultOk)
        // check for initial prefix
        res = getSlotForInitial(prefix, &slot);
    
    if (res == CallResultOk)
    {
        PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(prefix);
        SegmentNumber segmentNo = NdnRtcNamespace::getSegmentNumber(prefix);
        int nTotalSegments = prefix[-1].toFinalSegment()+1;
        
        if (slot->getState() == Slot::StateNew)
        { // mark assembling
            updateStat(slot->getState(), -1);
            slot->markAssembling(nTotalSegments, segmentSize_);
            updateStat(slot->getState(), 1);
        }
        
        if (slot->getState() == Slot::StateAssembling)
        {
            res = CallResultAssembling;
            updateStat(slot->getState(), -1);
            
            syncCs_.Enter();
            Slot::State slotState = slot->appendSegment(segmentNo,
                                                        data.getContent().size(),
                                                        data.getContent().buf());            
            syncCs_.Leave();
            
            updateStat(slotState, 1);
            // if we got 1st segment - notify regardless of other events
            if (slot->assembledSegmentsNumber() == 1)
                notifyBufferEventOccurred(packetNo, segmentNo,
                                          Event::FirstSegment, slot.get());
            
            switch (slotState)
            {
                case Slot::StateAssembling:
                    break;
                case Slot::StateReady: // slot ready event
                    notifyBufferEventOccurred(packetNo, segmentNo, Event::Ready,
                                              slot.get());
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
            TRACE("slot was booked but not marked assembling. id - %d, state %d",
                  slot->getBookingId(),
                  slot->getState());
        }
    }
    else
    {
        TRACE("trying to append segment to non-booked slot");
    }
    
    return res;
}

void FrameBuffer::notifySegmentTimeout(Name &prefix)
{
    shared_ptr<Slot> slot;
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(prefix);
    SegmentNumber segmentNo = NdnRtcNamespace::getSegmentNumber(prefix);
    
    if (getFrameSlot(prefix, &slot) == CallResultOk)
    {
        notifyBufferEventOccurred(packetNo, segmentNo, Event::Timeout,
                                  slot.get());
    }
    else
    {
        TRACE("can't notify slot %d - it was not found. current size: %d",
              packetNo, activeSlots_.size());
    }
}

FrameBuffer::Slot::State FrameBuffer::getState(PacketNumber frameNo)
{
    Slot::State state;
    shared_ptr<Slot> slot;
    Name prefix;
    
    if (getFrameSlot(frameNo, &slot, prefix) == CallResultOk)
        return slot->getState();
    
    return Slot::StateFree;
}

//void FrameBuffer::renameSlot(PacketNumber oldFrameNo, PacketNumber newFrameNo,
//                             bool useKeyNamespace)
//{
//    if (useKeyNamespace && oldFrameNo > 0 && newFrameNo > 0)
//    {
//        oldFrameNo = -oldFrameNo;
//        newFrameNo = -newFrameNo;
//    }
//    
//    EncodedImage encodedImage;
//    shared_ptr<Slot> slot;
//    
//    if (getFrameSlot(oldFrameNo, &slot, true) == CallResultOk)
//    {
//        slot->updateBookingId(newFrameNo);
//        frameSlotMapping_[newFrameNo] = slot;
//    }
//    
//    return;
//}

FrameBuffer::Event FrameBuffer::waitForEvents(int &eventsMask, unsigned int timeout)
{
    unsigned int wbrtcTimeout = (timeout == 0xffffffff)?WEBRTC_EVENT_INFINITE:timeout;
    bool stop = false;
    Event poppedEvent;
    
    memset(&poppedEvent, 0, sizeof(poppedEvent));
    poppedEvent.type_ = Event::Error;
    
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
        {
            stop = (bufferEvent_.Wait(wbrtcTimeout) != kEventSignaled);
        }
    }
    
    return poppedEvent;
}

unsigned int FrameBuffer::getStat(Slot::State state)
{
    if (statistics_.find(state) == statistics_.end())
        statistics_[state] = 0;
    
    return statistics_[state];
}

void FrameBuffer::reuseEvent(FrameBuffer::Event &event)
{
    TRACE("re-use event %d", event.type_);
    notifyBufferEventOccurred(event.packetNo_, event.segmentNo_,
                              event.type_, event.slot_);
}

//******************************************************************************
#pragma mark - private
void FrameBuffer::notifyBufferEventOccurred(PacketNumber packetNo,
                                            SegmentNumber segmentNo,
                                            Event::EventType eType,
                                            Slot *slot)
{
    Event ev;
    
    ev.packetNo_ = packetNo;
    ev.segmentNo_ = segmentNo;
    ev.type_ = eType;
    ev.slot_ = slot;
    
    bufferEventsRWLock_.AcquireLockExclusive();
    pendingEvents_.push_back(ev);
    bufferEventsRWLock_.ReleaseLockExclusive();
    
    // notify about event
    bufferEvent_.Set();
}

FrameBuffer::CallResult FrameBuffer::getFrameSlot(PacketNumber frameNo,
                                                  shared_ptr<Slot> *slot,
                                                  Name &prefix,
                                                  bool remove)
{
    map<Name, shared_ptr<Slot>>::iterator it = activeSlots_.begin();
    
    while (it != activeSlots_.end())
    {
        if (it->second->getFrameNumber() == frameNo)
        {
            TRACE("frameno match [%d] ==> [%s] (id %d state %d meta %d)",
                  frameNo,
                  it->first.toUri().c_str(),
                  it->second->getBookingId(),
                  it->second->getState(),
                  it->second->isMetaReady());
            break;
        }
        
        it++;
    }
    
    if (it != activeSlots_.end())
    {
        *slot = it->second;
        prefix = it->first;
        
        if (remove)
        {
            CriticalSectionScoped scopedCs(&syncCs_);
            activeSlots_.erase(it);
            
            dumpActiveSlots();
        }
        
        return CallResultOk;
    }
    
    return CallResultNotFound;
}

FrameBuffer::CallResult FrameBuffer::getFrameSlot(const Name &prefix,
                                                  shared_ptr<Slot> *slot,
                                                  bool remove)
{
    CallResult res = CallResultNotFound;
    map<Name, shared_ptr<Slot>>::iterator it;
    
    syncCs_.Enter();
    
    // try to find exact match
    it = activeSlots_.find(prefix);
    
    // try to find closest match in NDN canonical order
    if (it == activeSlots_.end())
    {
        TRACE("match check upper bound %s", prefix.toUri().c_str());
        it = activeSlots_.upper_bound(prefix);
    }
    
    if (it != activeSlots_.end() &&
        it->first.match(prefix))
    {
        TRACE("match [%s] ==> [%s]", prefix.toUri().c_str(),
              it->first.toUri().c_str());
        *slot = it->second;
        res = CallResultOk;
        
        if (remove)
        {
            CriticalSectionScoped scopedCs(&syncCs_);
            activeSlots_.erase(it);
            
            dumpActiveSlots();
        }
    }
    syncCs_.Leave();
    
    return res;
}

FrameBuffer::CallResult FrameBuffer::getSlotForInitial(const Name &prefix,
                                                       shared_ptr<Slot> *slot,
                                                       bool remove)
{
    CallResult res = CallResultNotFound;
    bool isDelta = NdnRtcNamespace::isDeltaFramesPrefix(prefix);
    bool isKey = NdnRtcNamespace::isKeyFramePrefix(prefix);
    
    map<Name, shared_ptr<Slot>>::iterator it = activeSlots_.begin();
    
    while (it != activeSlots_.end() &&
           res == CallResultNotFound)
    {
        Name p = it->first;
        
        bool isDeltaPrefix = NdnRtcNamespace::isDeltaFramesPrefix(p);
        bool isKeyPrefix = NdnRtcNamespace::isKeyFramePrefix(p);
        
        if (isDelta == isDeltaPrefix &&
            isKey == isKeyPrefix &&
            p.match(prefix))
        {
            res = CallResultOk;
            
            if (remove)
            {
                CriticalSectionScoped scopedCs(&syncCs_);
                activeSlots_.erase(it);
                
                dumpActiveSlots();
            }
        }
        it++;
    } // while
    
    return res;
}

void FrameBuffer::updateStat(Slot::State state, int change)
{
    if (statistics_.find(state) == statistics_.end())
        statistics_[state] = 0;
    
    statistics_[state] += change;
}

void FrameBuffer::resetData()
{
    bookingCounter_ = 0;
    forcedRelease_ = false;
    activeBookings_.clear();
}

BookingId FrameBuffer::getBookingIdForPrefix(const Name& prefix)
{
    BookingId bookingId = bookingCounter_++;
    activeBookings_.insert(bookingId);
    return bookingId;
#if 0
    // find frames component position
    Component streamsComp(NdnRtcNamespace::NameComponentUserStreams)
    Component framesComp(NdnRtcNamespace::NameComponentStreamFrames);
    Component keyFramesComp(NdnRtcNamespace::NameComponentStreamFramesKey);
    Component deltaFramesComp(NdnRtcNamespace::NameComponentStreamFramesDelta);
    
    int streamCompPos = -1;
    int framesCompPos = -1;
    int keyFramesCompPos = -1;
    int deltaFramesCompPos = -1;
    
    // start from the end - more chances to find faster
    for (int i = prefix.getComponentCount()-1; i >= 0; i--)
    {
        Component c = prefix.getComponent(i);
        
        if (c == streamsComp && streamsComp < 0)
        {
            streamsCompPos = i;
            break;
        }
        
        if (c == framesComp && framesCompPos < 0)
            framesCompPos = i;
        
        if (c == keyFramesComp && keyFramesCompPos < 0 && framesCompPos < 0)
            keyFramesCompPos = i;
        
        if (c == deltaFramesComp && deltaFramesCompPos < 0 && framesCompPos < 0)
            deltaFramesCompPos = i;
        
        // if we've found any component - exit
        if (framesCompPos > 0 ||
            keyFramesCompPos > 0 ||
            deltaFramesCompPos > 0 ||
            streamsCompPos)
            break;
    }
#endif
}

void FrameBuffer::markSlotFree(const Name &prefix, shared_ptr<Slot> &slot)
{
    updateStat(slot->getState(), -1);
    syncCs_.Enter();
    
    slot->markFree();
    activeSlots_.erase(prefix);
    freeSlots_.push_back(slot);
    activeBookings_.erase(slot->getBookingId());
    TRACE("added free slot. size %d", freeSlots_.size());
    
    dumpActiveSlots();
    
    syncCs_.Leave();
    
    updateStat(slot->getState(), 1);
    
    notifyBufferEventOccurred(slot->getFrameNumber(), -1, Event::FreeSlot, slot.get());
}

void FrameBuffer::dumpActiveSlots()
{
    map<Name, shared_ptr<Slot>>::iterator it2 = activeSlots_.begin();
    while (it2 != activeSlots_.end())
    {
        TRACE("[%s] - [%d %d %d %d]", it2->first.toUri().c_str(),
              it2->second->getBookingId(), it2->second->getState(),
              it2->second->isMetaReady(), it2->second->getFrameNumber());
        it2++;
    }
}


//******************************************************************************
// NEW API
//******************************************************************************
//******************************************************************************
// FrameBuffer::Slot::Segment
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::Slot::Segment::Segment()
{
    resetData();
}

ndnrtc::new_api::FrameBuffer::Slot::Segment::~Segment()
{
    
}


//******************************************************************************
#pragma mark - public
void
ndnrtc::new_api::FrameBuffer::Slot::Segment::discard()
{
    resetData();
}

void
ndnrtc::new_api::FrameBuffer::Slot::Segment::interestIssued
(const uint32_t& nonceValue)
{
    state_ = StatePending;
    requestTimeUsec_ = NdnRtcUtils::microsecondTimestamp();
    interestNonce_ = nonceValue;
    reqCounter_++;
}

void
ndnrtc::new_api::FrameBuffer::Slot::Segment::markMissed()
{
    state_ = StateMissing;
}

void
ndnrtc::new_api::FrameBuffer::Slot::Segment::dataArrived
(const SegmentData::SegmentMetaInfo& segmentMeta)
{
    state_ = StateFetched;
    arrivalTimeUsec_ = NdnRtcUtils::microsecondTimestamp();
    consumeTimeMs_ = segmentMeta.interestArrivalMs_;
    dataNonce_ = segmentMeta.interestNonce_;
    generationDelayMs_ = segmentMeta.generationDelayMs_;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::Segment::isOriginal()
{
    return (interestNonce_ != 0 && dataNonce_ == interestNonce_);
}

SegmentData::SegmentMetaInfo
ndnrtc::new_api::FrameBuffer::Slot::Segment::getMetadata() const
{
    SegmentData::SegmentMetaInfo meta;
    meta.generationDelayMs_ = generationDelayMs_;
    meta.interestNonce_ = interestNonce_;
    meta.interestArrivalMs_ = consumeTimeMs_;
    
    return meta;
}

//******************************************************************************
#pragma mark - private
void
ndnrtc::new_api::FrameBuffer::Slot::Segment::resetData()
{
    state_ = FrameBuffer::Slot::Segment::StateNotUsed;
    requestTimeUsec_ = -1;
    arrivalTimeUsec_ = -1;
    reqCounter_ = 0;
    dataNonce_ = 0;
    interestNonce_ = -1;
    generationDelayMs_ = -1;
    segmentNumber_ = -1;
    payloadSize_ = -1;
    consumeTimeMs_ = -1;
    prefix_ = Name();
}

//******************************************************************************
// FrameBuffer::Slot
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::Slot::Slot(unsigned int segmentSize):
state_(StateFree),
consistency_(Inconsistent),
segmentSize_(segmentSize),
accessCs_(*CriticalSectionWrapper::CreateCriticalSection())
{
    reset();
}

ndnrtc::new_api::FrameBuffer::Slot::~Slot() {}

//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::FrameBuffer::Slot::addInterest(const ndn::Interest &interest)
{
    if (state_ == StateReady || state_ == StateLocked)
        return RESULT_ERR;
    
    Name interestName = interest.getName();
    PacketNumber packetNumber = NdnRtcNamespace::getPacketNumber(interestName);
    
    if (packetSequenceNumber_ >= 0 && packetSequenceNumber_ != packetNumber)
        return RESULT_ERR;
    
    if (NdnRtcNamespace::isKeyFramePrefix(interestName))
        packetNamespace_ = Key;
    
    if (NdnRtcNamespace::isDeltaFramesPrefix(interestName))
        packetNamespace_ = Delta;
    
    if (packetNamespace_ == Unknown)
        return RESULT_ERR;
    
    if (packetNumber >= 0)
        packetSequenceNumber_ = packetNumber;
    
    SegmentNumber segmentNumber = NdnRtcNamespace::getSegmentNumber(interestName);
    shared_ptr<Segment> reservedSegment = prepareFreeSegment(segmentNumber);
    
    if (reservedSegment.get())
    {
        reservedSegment->interestIssued(NdnRtcUtils::blobToNonce(interest.getNonce()));
        reservedSegment->setNumber(segmentNumber);
        
        // rightmost child prefix
        if (packetNumber < 0)
            rightmostSegment_ = reservedSegment;
        else
        {
            CriticalSectionScoped scopedCs(&accessCs_);
            activeSegments_[segmentNumber] = reservedSegment;
        }
        
        if (state_ == StateFree)
        {
            NdnRtcNamespace::trimSegmentNumber(interestName, slotPrefix_);
            state_ = StateNew;
        }
        
        nSegmentsPending_++;
    }
    else
        return RESULT_WARN;
    
    return RESULT_OK;
}

int
ndnrtc::new_api::FrameBuffer::Slot::markMissing(const ndn::Interest &interest)
{
    if (state_ == StateLocked)
        return RESULT_ERR;
    
    Name interestName = interest.getName();
    PacketNumber packetNumber = NdnRtcNamespace::getPacketNumber(interestName);
    SegmentNumber segmentNumber = NdnRtcNamespace::getSegmentNumber(interestName);
    
    if (packetNumber < 0 &&
        segmentNumber < 0 &&
        rightmostSegment_.get() != nullptr)
    {
        rightmostSegment_->markMissed();
        return RESULT_OK;
    }

    int res = RESULT_ERR;
    shared_ptr<Segment> segment = getSegment(segmentNumber);
    
    if (segment.get())
    {
        if (segment->getState() == Segment::StatePending)
        {
            res = RESULT_OK;
            nSegmentsMissing_++;
            nSegmentsPending_--;
            segment->markMissed();
        }
        else
            res = RESULT_WARN;
    }

    return res;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::Slot::appendData(const ndn::Data &data)
{
    if ((state_ != StateAssembling && state_ != StateNew) ||
        nSegmentsPending_ == 0)
        return state_;
    
    Name dataName = data.getName();
    PrefixMetaInfo prefixMeta;
    
    if (RESULT_GOOD(PrefixMetaInfo::extractMetadata(dataName, prefixMeta)))
    {
        PacketNumber packetNumber = NdnRtcNamespace::getPacketNumber(dataName);
        SegmentNumber segmentNumber = NdnRtcNamespace::getSegmentNumber(dataName);
        
        fixRightmost(packetNumber, segmentNumber);
        updateConsistencyWithMeta(packetNumber, prefixMeta);
        
        SegmentData segmentData;
        
        if (RESULT_GOOD(SegmentData::segmentDataFromRaw(data.getContent().size(),
                                                        data.getContent().buf(),
                                                        segmentData)))
        {
            SegmentData::SegmentMetaInfo segmentMeta = segmentData.getMetadata();

            addData(segmentData.getSegmentData(), segmentData.getSegmentDataLength(),
                    segmentNumber, prefixMeta.totalSegmentsNum_);

            if (segmentNumber == 0)
                updateConsistencyFromHeader();

            shared_ptr<Segment> segment = getActiveSegment(segmentNumber);
            segment->setDataPtr(getSegmentDataPtr(segmentData.getSegmentDataLength(),
                                                  segmentNumber));
            segment->setNumber(segmentNumber);
            segment->setPayloadSize(segmentData.getSegmentDataLength());
            segment->dataArrived(segmentMeta);
            
            nSegmentsPending_--;
            nSegmentsReady_++;
            recentSegment_ = segment;
            
            vector<shared_ptr<Segment>> fetchedSegments =
                getSegments(Segment::StateFetched);

            if (nSegmentsReady_ == nSegmentsTotal_ &&
                fetchedSegments.size() != prefixMeta.totalSegmentsNum_)
                LogError("") << "alarma "<< endl;
            
            if (fetchedSegments.size() == prefixMeta.totalSegmentsNum_)
                state_ = StateReady;
            else
            {
                if (fetchedSegments.size() < prefixMeta.totalSegmentsNum_)
                    state_ = StateAssembling;
            }
            
//            if (nSegmentsPending_)
//                state_ = StateAssembling;
//            else
//                state_ = StateReady;
        }
        else
        {
            LogError("") << "couldn't get data from segment" << endl;
        }
    }
    else
    {
        LogError("") << "couldn't get metadata from packet" << endl;
    }
    
    return state_;
}

int
ndnrtc::new_api::FrameBuffer::Slot::reset()
{
    if (state_ == StateLocked)
        return RESULT_ERR;
    
    state_ = StateFree;
    consistency_ = Inconsistent;
    packetNamespace_ = Unknown;
    packetPlaybackNumber_ = -1;
    packetSequenceNumber_ = -1;
    keySequenceNumber_ = -1;
    producerTimestamp_ = -1;
    playbackDeadline_ = -1;
    packetRate_ = -1;
    nSegmentsTotal_ = 0;
    nSegmentsReady_ = 0;
    nSegmentsPending_ = 0;
    nSegmentsMissing_ = 0;
    slotPrefix_ = Name();
    recentSegment_.reset();
    assembledSize_ = 0;
    
    {
        CriticalSectionScoped scopedCs(&accessCs_);
        // clear all active segments if any
        if (activeSegments_.size())
        {
            map<SegmentNumber, shared_ptr<Segment>>::iterator it;
            
            for (it = activeSegments_.begin(); it != activeSegments_.end(); ++it)
            {
                it->second->discard();
                freeSegments_.push_back(it->second);
            }
            activeSegments_.clear();
        }
    }
    
    // clear rightmost segment if any
    if (rightmostSegment_.get())
    {
        freeSegments_.push_back(rightmostSegment_);
        rightmostSegment_.reset();
    }
    
    return RESULT_OK;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::PlaybackComparator::operator()
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot1,
 const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot2) const
{
    bool result = false;
    
    if (slot1.get() && slot2.get())
    {
        PacketNumber p1 = slot1->getSequentialNumber();
        PacketNumber p2 = slot2->getSequentialNumber();
        
        if ((slot1->getNamespace() == slot2->getNamespace()) &&
            (p1 >= 0 && p2 >= 0))
        {
            // compare based on namespace packet sequence number
            assert(p1>=0); assert(p2>=0);
            result = !(p1 < p2);
        }
        else
        {
            // otherwise - compare based on prefixes canonical order
            // "key" will be always greater than delta - that's ok
            result = !(slot1->getPrefix() < slot2->getPrefix());
        }
    } // if slots are Ok
    
    return result;
}

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>
ndnrtc::new_api::FrameBuffer::Slot::prepareFreeSegment(SegmentNumber segNo)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    shared_ptr<Segment> freeSegment(NULL);
    map<SegmentNumber, shared_ptr<Segment>>::iterator
        it = activeSegments_.find(segNo);
    
    if (it != activeSegments_.end())
    {
        if (it->second->getState() == Segment::StateMissing)
        {
            freeSegment = it->second;
            nSegmentsMissing_--;
        }
    }
    else
    {
        if (freeSegments_.size())
        {
            freeSegment = freeSegments_[freeSegments_.size()-1];
            freeSegments_.pop_back();
        }
        else
            freeSegment.reset(new Segment());

        if (segNo >= 0)
            freeSegment->setPrefix(Name(getPrefix()).appendSegment(segNo));
        else
            freeSegment->setPrefix(Name(getPrefix()));
    }
    
    return freeSegment;
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>&
ndnrtc::new_api::FrameBuffer::Slot::getActiveSegment(SegmentNumber segmentNumber)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    map<SegmentNumber, shared_ptr<Segment>>::iterator
    it = activeSegments_.find(segmentNumber);
    
    if (it != activeSegments_.end())
        return it->second;
    
    // should never happen
    assert(false);
    shared_ptr<Segment> nullSeg(NULL);
    return nullSeg;
}

void
ndnrtc::new_api::FrameBuffer::Slot::fixRightmost(PacketNumber packetNumber,
                                                 SegmentNumber segmentNumber)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    map<SegmentNumber, shared_ptr<Segment>>::iterator
    it = activeSegments_.find(segmentNumber);
    
    if (it == activeSegments_.end() &&
        rightmostSegment_.get())
    {
        setPrefix(slotPrefix_.append(NdnRtcUtils::componentFromInt(packetNumber)));

        Name segmentPrefix(slotPrefix_);
        rightmostSegment_->setPrefix(segmentPrefix.appendSegment(segmentNumber));
        
        activeSegments_[segmentNumber] = rightmostSegment_;
        rightmostSegment_.reset();
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::prepareStorage(unsigned int segmentSize,
                                                   unsigned int nSegments)
{
    if (allocatedSize_ < segmentSize*nSegments)
    {
        slotData_ = (unsigned char*)realloc(slotData_, segmentSize*nSegments);
        allocatedSize_ = segmentSize*nSegments;
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::addData(const unsigned char* segmentData,
                                            unsigned int segmentSize,
                                            SegmentNumber segNo,
                                            unsigned int totalSegNo)
{
    // prepare storage with the known segment size, instead of the actual one!
    unsigned int realSegmentSize = segmentSize_-SegmentData::getHeaderSize();
    
    prepareStorage(realSegmentSize, totalSegNo);
    memcpy(slotData_+segNo*realSegmentSize, segmentData, segmentSize);
    assembledSize_ += segmentSize;
}

unsigned char*
ndnrtc::new_api::FrameBuffer::Slot::getSegmentDataPtr(unsigned int segmentSize,
                                                      unsigned int segmentNumber)
{
    if (allocatedSize_ > segmentSize*segmentNumber)
    {
        return slotData_+segmentSize*segmentNumber;
    }
    
    return NULL;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>
ndnrtc::new_api::FrameBuffer::Slot::getSegments(int segmentStateMask) 
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    std::vector<shared_ptr<Segment>> foundSegments;
    map<SegmentNumber, shared_ptr<Segment>>::iterator it;
    
    for (it = activeSegments_.begin(); it != activeSegments_.end(); ++it)
        if (it->second->getState() & segmentStateMask)
            foundSegments.push_back(it->second);
    
    if (rightmostSegment_.get() &&
        rightmostSegment_->getState()&segmentStateMask)
        foundSegments.push_back(rightmostSegment_);
    
    return foundSegments;
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>
ndnrtc::new_api::FrameBuffer::Slot::getSegment(SegmentNumber segNo) const
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    shared_ptr<Segment> segment(NULL);
    map<SegmentNumber, shared_ptr<Segment>>::const_iterator
    it = activeSegments_.find(segNo);
    
    if (it != activeSegments_.end())
        segment = it->second;
    
    return segment;
}

void
ndnrtc::new_api::FrameBuffer::Slot::updateConsistencyWithMeta(const PacketNumber& sequenceNumber,
                                                              const PrefixMetaInfo &prefixMeta)
{
    // avoid unnecessary updates
    if (consistency_ & PrefixMeta)
        return;
    
    consistency_ |= PrefixMeta;
    
    packetSequenceNumber_ = sequenceNumber;
    packetPlaybackNumber_ = prefixMeta.playbackNo_;
    keySequenceNumber_ = prefixMeta.keySequenceNo_;
    nSegmentsTotal_ = prefixMeta.totalSegmentsNum_;
    
    initMissingSegments();
}

void
ndnrtc::new_api::FrameBuffer::Slot::initMissingSegments()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    SegmentNumber segNo = 0;
    
    while (activeSegments_.size() < nSegmentsTotal_)
    {
        if (activeSegments_.find(segNo) == activeSegments_.end())
        {
            shared_ptr<Segment> segment(new Segment());
            segment->setNumber(segNo);
            segment->markMissed();
            segment->setPrefix(Name(getPrefix()).appendSegment(segNo));
            
            activeSegments_[segNo] = segment;
            nSegmentsMissing_++;
        }
        segNo++;
    }
}

bool
ndnrtc::new_api::FrameBuffer::Slot::updateConsistencyFromHeader()
{
    if (consistency_&HeaderMeta)
        return false;
    
    consistency_ |= HeaderMeta;
    
    PacketData *packetData;
    PacketData::packetFromRaw(allocatedSize_, slotData_, &packetData);
    
    packetRate_ = packetData->getMetadata().packetRate_;
    producerTimestamp_ = packetData->getMetadata().timestamp_;
    
    return true;
}

//******************************************************************************
// PlaybackQueue
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::PlaybackQueue::PlaybackQueue
(unsigned int playbackRate):
playbackRate_(playbackRate),
accessCs_(*CriticalSectionWrapper::CreateCriticalSection()),
comparator_(FrameBuffer::Slot::PlaybackComparator())
{}

//******************************************************************************
#pragma mark - public
int64_t
ndnrtc::new_api::FrameBuffer::PlaybackQueue::getPlaybackDuration()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    int64_t playbackDurationMs = 0;
    
    if (this->size() >= 1)
    {
        std::vector<shared_ptr<FrameBuffer::Slot>>::iterator it = this->begin();
        shared_ptr<FrameBuffer::Slot>& slot1 = *it;
    
        while (++it != this->end())
        {
            shared_ptr<FrameBuffer::Slot>& slot2 = *it;
            
            // if header metadata is available - we have frame timestamp
            if (slot1->getConsistencyState()&Slot::HeaderMeta &&
                slot2->getConsistencyState()&Slot::HeaderMeta)
            {
                playbackDurationMs += (slot2->getProducerTimestamp() - slot1->getProducerTimestamp());
            }
            else
            {
                // otherwise - infer playback duration from the producer rate
                playbackDurationMs += getInferredDuration();
            }
            
            slot1 = slot2;
        }
        
        // add infered duration for the last frame always
        playbackDurationMs += getInferredDuration();
    }
    
    return playbackDurationMs;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::pushSlot
(const shared_ptr<FrameBuffer::Slot> &slot)
{
    CriticalSectionScoped scopedCs(&accessCs_);

//    {
//        int j = 0;
//        std::vector<shared_ptr<FrameBuffer::Slot>>::iterator it = this->begin();
//        LogTrace("") << "pushing new " << slot->getPrefix() << endl;
//        while (it != this->end())
//        {
//            LogTrace("") << j++ << " slot " << (*it)->getPrefix() << endl;
//            it++;
//        }
//    }
    
    this->push_back(slot);
    std::push_heap(this->begin(), this->end(), comparator_);

//    {
//        std::vector<shared_ptr<FrameBuffer::Slot>>::iterator it = this->begin();
//        int i = 0;
//        LogTrace("") << "-- pushed" << endl;
//        while (it != this->end())
//        {
//            LogTrace("") << i++ << " slot " << (*it)->getPrefix() << endl;
//            it++;
//        }
//    }
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::PlaybackQueue::popSlot()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> slot(NULL);
    
    if (0 != this->size())
    {
        std::pop_heap(this->begin(), this->end(), comparator_);
        slot = this->front();
        this->pop_back();
    }
    
    return slot;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackRate(double playbackRate)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    playbackRate_ = playbackRate;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::clear()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>>::clear();
}

//******************************************************************************
// FrameBuffer
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::FrameBuffer(shared_ptr<const ndnrtc::new_api::FetchChannel> &fetchChannel):
fetchChannel_(fetchChannel),
state_(Invalid),
targetSizeMs_(-1),
estimatedSizeMs_(-1),
isEstimationNeeded_(true),
playbackQueue_(fetchChannel_->getParameters().producerRate),
syncCs_(*CriticalSectionWrapper::CreateCriticalSection()),
bufferEvent_(*EventWrapper::Create()),
forcedRelease_(false),
bufferEventsRWLock_(*RWLockWrapper::CreateRWLock())
{
}

ndnrtc::new_api::FrameBuffer::~FrameBuffer()
{
    
}
//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::FrameBuffer::reset()
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    // look through all active slots and release them if they're not locked

    // clear all events
    bufferEventsRWLock_.AcquireLockExclusive();
    pendingEvents_.clear();
    bufferEventsRWLock_.ReleaseLockExclusive();

    LogTrace(fetchChannel_->getLogFile()) << "flush. active "
        << activeSlots_.size() << ". pending " << pendingEvents_.size()
        << ". free " << freeSlots_.size() << endl;
    
    for (map<Name, shared_ptr<Slot>>::iterator it = activeSlots_.begin();
         it != activeSlots_.end(); ++it)
    {
        shared_ptr<Slot> slot = it->second;
        if (slot->getState() != Slot::StateLocked)
        {
            freeSlots_.push_back(slot);
            slot->reset();
            addBufferEvent(Event::FreeSlot, slot);
        }
        else
            LogWarn(fetchChannel_->getLogFile()) << "slot locked " << it->first << endl;
    }
    
    resetData();
    
    return RESULT_OK;
}

void
ndnrtc::new_api::FrameBuffer::release()
{
    forcedRelease_ = true;
    bufferEvent_.Set();
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::interestIssued(const ndn::Interest &interest)
{
    shared_ptr<Slot> reservedSlot = getSlot(interest.getName(), false, true);
    
    // check if slot is already reserved
    if (reservedSlot.get())
    {
      if (RESULT_GOOD(reservedSlot->addInterest(interest)))
      {
          isEstimationNeeded_ = true;
          
          LogTrace(fetchChannel_->getLogFile())
          << "pending " << interest.getName() << endl;
          
          return reservedSlot->getState();
      }
      else
          LogWarn(fetchChannel_->getLogFile())
          << "error adding " << interest.getName() << endl;
    }
    else
        return reserveSlot(interest);

    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::newData(const ndn::Data &data)
{
    shared_ptr<Slot> slot = getSlot(data.getName(), false, true);
    
    // if not reserved yet
    if (slot.get())
    {
        Slot::State oldState = slot->getState();
        
        if (oldState == Slot::StateNew ||
            oldState == Slot::StateAssembling)
        {
            Slot::State newState = slot->appendData(data);
            
            LogTrace(fetchChannel_->getLogFile()) << "appended "
            << data.getName() << " (" << slot->getAssembledLevel()*100 << "%) with result " << Slot::stateToString(newState) << endl;
            
            if (slot->getAssembledLevel() == 1. && newState == Slot::StateAssembling)
                LogTrace("") << "alarma" << endl;
            
            if (oldState != newState ||
                newState == Slot::StateAssembling)
            {
                if (slot->getConsistencyState()&Slot::HeaderMeta)
                    updateCurrentRate(slot->getPacketRate());
                
                // check for 1st segment
                if (oldState == Slot::StateNew)
                    addBufferEvent(Event::FirstSegment, slot);
                
                // check for ready event
                if (newState == Slot::StateReady)
                {
                    LogTrace("buffer.log") << "ready " << slot->getPrefix() << endl;
                    addBufferEvent(Event::Ready, slot);
                }
                
                return newState;
            }
        }
        else
            LogError(fetchChannel_->getLogFile()) << "error appending " << data.getName()
            << ". state: " << Slot::stateToString(oldState) << endl;
    }
    
    LogWarn(fetchChannel_->getLogFile()) << "no reservation for "
    << data.getName() << " state " << FrameBuffer::stateToString(state_) << endl;

    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::freeSlot(const ndn::Name &prefix)
{
    shared_ptr<Slot> slot = getSlot(prefix, true, true);
    
    if (slot.get())
    {
        if (slot->getState() != Slot::StateLocked)
        {
            CriticalSectionScoped scopedCs(&syncCs_);
            
            slot->reset();
            freeSlots_.push_back(slot);
            
            return slot->getState();
        }
    }
    
    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Event
ndnrtc::new_api::FrameBuffer::waitForEvents(int eventsMask, unsigned int timeout)
{
    unsigned int wbrtcTimeout = (timeout == 0xffffffff)?WEBRTC_EVENT_INFINITE:timeout;
    bool stop = false;
    Event poppedEvent;
    
    memset(&poppedEvent, 0, sizeof(poppedEvent));
    poppedEvent.type_ = Event::Error;
    
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
        {
            stop = (bufferEvent_.Wait(wbrtcTimeout) != kEventSignaled);
        }
    }
    
    return poppedEvent;
}

void
ndnrtc::new_api::FrameBuffer::recycleOldSlots()
{
    assert(false);
}

int64_t
ndnrtc::new_api::FrameBuffer::getEstimatedBufferSize()
{
    if (isEstimationNeeded_)
    {
        estimateBufferSize();
        isEstimationNeeded_ = false;
    }

    return estimatedSizeMs_;
}

void
ndnrtc::new_api::FrameBuffer::acquireSlot(ndnrtc::PacketData **packetData)
{

}

int
ndnrtc::new_api::FrameBuffer::releaseAcquiredSlot()
{
    return 0;
}

void
ndnrtc::new_api::FrameBuffer::recycleEvent(ndnrtc::new_api::FrameBuffer::Event &event)
{
    addBufferEvent(event.type_, event.slot_);
}

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::getSlot(const Name& prefix, bool remove,
                                      bool shouldMatch)
{
    if (isWaitingForRightmost_)
        fixRightmost(prefix);
    
    shared_ptr<Slot> slot(NULL);
    Name lookupPrefix;
    
    if (getLookupPrefix(prefix, lookupPrefix))
    {
        CriticalSectionScoped scopedCs(&syncCs_);
        std::map<Name, shared_ptr<Slot>>::iterator it = activeSlots_.find(lookupPrefix);
        
        if (it != activeSlots_.end())
        {
            slot = it->second;
            
            if (remove)
                activeSlots_.erase(it);
        }
    }
    
    return slot;
}

int
ndnrtc::new_api::FrameBuffer::setSlot(const ndn::Name &prefix,
                                      shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot)
{
    Name lookupPrefix;
    
    if (getLookupPrefix(prefix, lookupPrefix))
    {
        CriticalSectionScoped scopedCs(&syncCs_);
        activeSlots_[lookupPrefix] = slot;
        slot->setPrefix(lookupPrefix);
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void
ndnrtc::new_api::FrameBuffer::estimateBufferSize()
{
    CriticalSectionScoped scopedCs(&syncCs_);
    estimatedSizeMs_ = playbackQueue_.getPlaybackDuration();
}

void
ndnrtc::new_api::FrameBuffer::resetData()
{
    activeSlots_.clear();
    playbackQueue_.clear();
    
    state_ = Invalid;
    addStateChangedEvent(state_);
    
    isWaitingForRightmost_ = true;
    targetSizeMs_ = -1;
    estimatedSizeMs_ = -1;
    isEstimationNeeded_ = true;
}

bool
ndnrtc::new_api::FrameBuffer::getLookupPrefix(const Name& prefix,
                                              Name& lookupPrefix)
{
    bool res = false;
    
    // check if it is ok
    if (NdnRtcNamespace::isKeyFramePrefix(prefix) ||
        NdnRtcNamespace::isDeltaFramesPrefix(prefix))
    {
        // check if it has packet number and segment number
        if (NdnRtcNamespace::getPacketNumber(prefix) >= 0 &&
            NdnRtcNamespace::getSegmentNumber(prefix) >= 0)
                res = (NdnRtcNamespace::trimSegmentNumber(prefix, lookupPrefix) >= 0);
        else
        {
            lookupPrefix = prefix;
            res = true;
        }
    }
    else
        res = false;
    
//    if (res)
//        LogTrace(fetchChannel_->getLogFile()) << "lookup " << lookupPrefix
//            << " for prefix " << prefix << endl;
//    else
//        LogWarn(fetchChannel_->getLogFile()) << "wrong slot prefix "
//            << prefix << endl;
    
    return res;
}

void
ndnrtc::new_api::FrameBuffer::addBufferEvent(Event::EventType type,
                                             shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot)
{
    Event ev;
    
    ev.type_ = type;
    ev.slot_ = slot;
    
    bufferEventsRWLock_.AcquireLockExclusive();
    pendingEvents_.push_back(ev);
    bufferEventsRWLock_.ReleaseLockExclusive();
    
    bufferEvent_.Set();
}

void
ndnrtc::new_api::FrameBuffer::addStateChangedEvent(ndnrtc::new_api::FrameBuffer::State newState)
{
    shared_ptr<Slot> nullPtr;
    addBufferEvent(Event::StateChanged, nullPtr);
}

void
ndnrtc::new_api::FrameBuffer::initialize()
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    while (freeSlots_.size() < fetchChannel_->getParameters().bufferSize)
    {
        shared_ptr<Slot> slot(new Slot(fetchChannel_->getParameters().segmentSize));
        
        freeSlots_.push_back(slot);
        addBufferEvent(Event::FreeSlot, slot);
    }
    
    setTargetSize(fetchChannel_->getParameters().jitterSize);
    
    playbackQueue_.updatePlaybackRate(fetchChannel_->getParameters().producerRate);
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::reserveSlot(const ndn::Interest &interest)
{
    if (freeSlots_.size() > 0)
    {
        shared_ptr<Slot> reservedSlot = freeSlots_.back();
        
        if (RESULT_GOOD(reservedSlot->addInterest(interest)))
        {
            setSlot(interest.getName(), reservedSlot);
            freeSlots_.pop_back();
            playbackQueue_.pushSlot(reservedSlot);
            isEstimationNeeded_ = true;
            
            LogTrace(fetchChannel_->getLogFile()) << "reserved "
            << reservedSlot->getPrefix() << endl;
        }
        else
            LogWarn(fetchChannel_->getLogFile()) << "error reserve "
            << interest.getName() << endl;
        
        return reservedSlot->getState();
    }
    
    return Slot::StateFree;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>>
ndnrtc::new_api::FrameBuffer::getSlots(int slotStateMask)
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    vector<shared_ptr<FrameBuffer::Slot>> slots;
    map<Name, shared_ptr<Slot>>::iterator it;
    
    for (it = activeSlots_.begin(); it != activeSlots_.end(); ++it)
    {
        if (it->second->getState() & slotStateMask)
            slots.push_back(it->second);
    }
    
    return slots;
}

void
ndnrtc::new_api::FrameBuffer::fixRightmost
(const Name& dataName)
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    Name rightmostPrefix;
    NdnRtcNamespace::trimPacketNumber(dataName, rightmostPrefix);
 
    if (activeSlots_.find(rightmostPrefix) != activeSlots_.end())
    {
        shared_ptr<Slot> slot = activeSlots_[rightmostPrefix];
        activeSlots_.erase(rightmostPrefix);
        
        Name lookupPrefix;
        getLookupPrefix(dataName, lookupPrefix);
        
        activeSlots_[lookupPrefix] = slot;
        
        isWaitingForRightmost_ = false;
        
        LogTrace(fetchChannel_->getLogFile()) << "fixed righmost entry" << endl;
    }
}
