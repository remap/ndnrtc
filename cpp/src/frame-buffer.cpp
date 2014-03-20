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
#include "rtt-estimation.h"

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
    assert(nonceValue != 0);
    
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
    
    if (isOriginal())
        RttEstimation::sharedInstance().updateEstimation(requestTimeUsec_/1000,
                                                         arrivalTimeUsec_/1000,
                                                         generationDelayMs_);
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
segmentSize_(segmentSize)
//accessCs_(*CriticalSectionWrapper::CreateCriticalSection())
{
    reset();
}

ndnrtc::new_api::FrameBuffer::Slot::~Slot() {}

//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::FrameBuffer::Slot::addInterest(ndn::Interest &interest)
{
    if (state_ == StateReady || state_ == StateLocked)
        return RESULT_ERR;
    
    Name interestName = interest.getName();
    PacketNumber packetNumber = NdnRtcNamespace::getPacketNumber(interestName);
    
    if (packetSequenceNumber_ >= 0 && packetSequenceNumber_ != packetNumber)
        return RESULT_ERR;
    
    if (packetNumber >= 0)
        packetSequenceNumber_ = packetNumber;
    
    SegmentNumber segmentNumber = NdnRtcNamespace::getSegmentNumber(interestName);
    shared_ptr<Segment> reservedSegment = prepareFreeSegment(segmentNumber);
    
    if (reservedSegment.get())
    {
        interest.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
        reservedSegment->interestIssued(NdnRtcUtils::blobToNonce(interest.getNonce()));
        reservedSegment->setNumber(segmentNumber);
        
        // rightmost child prefix
        if (packetNumber < 0)
            rightmostSegment_ = reservedSegment;
        else
        {
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
    PacketNumber packetNumber = NdnRtcNamespace::getPacketNumber(dataName);
    SegmentNumber segmentNumber = NdnRtcNamespace::getSegmentNumber(dataName);

    if (!rightmostSegment_.get() &&
        packetNumber != packetSequenceNumber_)
        return StateFree;
    
    PrefixMetaInfo prefixMeta;
    
    if (RESULT_GOOD(PrefixMetaInfo::extractMetadata(dataName, prefixMeta)))
    {
        fixRightmost(packetNumber, segmentNumber);
        updateConsistencyWithMeta(packetNumber, prefixMeta);
        
        SegmentData segmentData;
        
        if (RESULT_GOOD(SegmentData::segmentDataFromRaw(data.getContent().size(),
                                                        data.getContent().buf(),
                                                        segmentData)))
        {
            SegmentData::SegmentMetaInfo segmentMeta = segmentData.getMetadata();

            LogTrace("packet-meta.log")
            << " packet " << packetNumber << " seg " << segmentNumber << " "
            << "generation " << segmentMeta.generationDelayMs_ << " "
            << "int arrival " << segmentMeta.interestArrivalMs_ << endl;
            
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
    pairedSequenceNumber_ = -1;
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
        
        // clear rightmost segment if any
        if (rightmostSegment_.get())
        {
            freeSegments_.push_back(rightmostSegment_);
            rightmostSegment_.reset();
        }
    }
    
    return RESULT_OK;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::PlaybackComparator::operator()
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot1,
 const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot2) const
{
    bool ascending = false;
    
    // if either frame has timestamp info - use this for checking
    if (slot1->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::HeaderMeta ||
        slot2->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::HeaderMeta)
    {
        if (slot1->getConsistencyState() == slot2->getConsistencyState())
        {
            ascending = slot1->getProducerTimestamp() < slot2->getProducerTimestamp();
        }
        else
        {
            ascending = (slot1->getConsistencyState() &
                         ndnrtc::new_api::FrameBuffer::Slot::HeaderMeta);
        }
    }
    else
    {
        if (slot1->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta &&
            slot2->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta)
        {
            ascending = slot1->getPlaybackNumber() < slot2->getPlaybackNumber();
        }
        else
        {
            if (slot1->getNamespace() == slot2->getNamespace())
            {
                ascending = slot1->getSequentialNumber() < slot2->getSequentialNumber();
            }
            else
            {
                if (slot1->getConsistencyState() != slot2->getConsistencyState() &&
                    (slot1->getNamespace() == ndnrtc::new_api::FrameBuffer::Slot::Delta &&
                     slot1->getConsistencyState()&ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta ||
                     slot2->getNamespace() == ndnrtc::new_api::FrameBuffer::Slot::Delta &&
                     slot2->getConsistencyState()&ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta))
                {
                    if (slot1->getNamespace() == ndnrtc::new_api::FrameBuffer::Slot::Delta)
                    {
                        ascending = slot1->getPairedFrameNumber() < slot2->getSequentialNumber();
                    }
                    else
                    {
                        ascending = slot1->getSequentialNumber() < slot2->getPairedFrameNumber();
                    }
                }
                else
                {
                    // when slot1 and slot2 have different namespaces
                    // comparison slot1 < slot2 will be true if slot1 comes from a
                    // Delta namespace
                    ascending = (slot1->getNamespace() == FrameBuffer::Slot::Delta);
                }
            }
        }
    }
    
    return ascending^inverted_;
}

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>
ndnrtc::new_api::FrameBuffer::Slot::prepareFreeSegment(SegmentNumber segNo)
{
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

void
ndnrtc::new_api::FrameBuffer::Slot::setPrefix(const ndn::Name &prefix)
{
    if (NdnRtcNamespace::isKeyFramePrefix(prefix))
        packetNamespace_ = Key;
    else if (NdnRtcNamespace::isDeltaFramesPrefix(prefix))
        packetNamespace_ = Delta;
    else
        packetNamespace_ = Unknown;
    
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(prefix);
    packetSequenceNumber_ = packetNo;
    
    slotPrefix_ = prefix;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>
ndnrtc::new_api::FrameBuffer::Slot::getSegments(int segmentStateMask) 
{
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
    pairedSequenceNumber_ = prefixMeta.pairedSequenceNo_;
    nSegmentsTotal_ = prefixMeta.totalSegmentsNum_;
    
    initMissingSegments();
}

void
ndnrtc::new_api::FrameBuffer::Slot::initMissingSegments()
{
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
    
    // if there are more pending interests than we expect
    while (activeSegments_.size() > nSegmentsTotal_) {
        activeSegments_.erase(--activeSegments_.end());
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

std::string
ndnrtc::new_api::FrameBuffer::Slot::dump()
{
    std::stringstream dump;
    
    dump
    << (getNamespace() == Slot::Delta?"D":"K") << " "
    << setw(7) << getSequentialNumber() << " "
    << setw(10) << getProducerTimestamp() << " "
    << setw(3) << getAssembledLevel()*100 << "% "
    << setw(5) << getPairedFrameNumber() << " "
    << Slot::toString(getConsistencyState()) << " "
    << setw(3) << getPlaybackDeadline() 
    << setw(2) << nSegmentsTotal_ << "/" << nSegmentsReady_
    << "/" << nSegmentsPending_ << "/" << nSegmentsMissing_;
    
    return dump.str();
}

//******************************************************************************
// PlaybackQueue
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::PlaybackQueue::PlaybackQueue
(double playbackRate):
playbackRate_(playbackRate),
accessCs_(*CriticalSectionWrapper::CreateCriticalSection()),
comparator_(FrameBuffer::Slot::PlaybackComparator(false))
{}

//******************************************************************************
#pragma mark - public
int64_t
ndnrtc::new_api::FrameBuffer::PlaybackQueue::getPlaybackDuration(bool estimate)
{
    CriticalSectionScoped scopedCs(&accessCs_);

    int64_t playbackDurationMs = 0;
    
    if (this->size() >= 1)
    {
        PlaybackQueueBase::iterator it = this->begin();
        shared_ptr<FrameBuffer::Slot> slot1 = *it;
    
        while (++it != this->end())
        {
            shared_ptr<FrameBuffer::Slot> slot2 = *it;
            
            // if header metadata is available - we have frame timestamp
            if (slot1->getConsistencyState()&Slot::HeaderMeta &&
                slot2->getConsistencyState()&Slot::HeaderMeta)
            {
                playbackDurationMs += (slot2->getProducerTimestamp() - slot1->getProducerTimestamp());
            }
            else
            {
                // otherwise - infer playback duration from the producer rate
                playbackDurationMs += (estimate)?getInferredFrameDuration():0;
            }
            
            slot1 = slot2;
        }
        
        if (estimate ||
            !estimate && slot1->getConsistencyState() != Slot::Consistent)
            // add infered duration for the last frame always
            playbackDurationMs += getInferredFrameDuration();
    }
    
    return playbackDurationMs;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackDeadlines()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
    int64_t playbackDeadlineMs = 0;
    
    if (this->size() >= 1)
    {
        PlaybackQueueBase::iterator it = this->begin();
        shared_ptr<FrameBuffer::Slot> slot1 = *it;
        
        while (++it != this->end())
        {
            slot1->setPlaybackDeadline(playbackDeadlineMs);
            
            shared_ptr<FrameBuffer::Slot> slot2 = *it;
            
            // if header metadata is available - we have frame timestamp
            if (slot1->getConsistencyState()&Slot::HeaderMeta &&
                slot2->getConsistencyState()&Slot::HeaderMeta)
            {
                playbackDeadlineMs += (slot2->getProducerTimestamp() - slot1->getProducerTimestamp());
            }
            else
            {
                // otherwise - infer playback duration from the producer rate
                playbackDeadlineMs += getInferredFrameDuration();
            }
            
            slot1 = slot2;
        }
        
        // set deadline for the last frame
        slot1->setPlaybackDeadline(playbackDeadlineMs);
    }
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::pushSlot
(const shared_ptr<FrameBuffer::Slot> &slot)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    
//    this->insert(slot);
    this->push_back(slot);
    sort();

    {
        LogTrace("playback-queue.log") << "▼push[" << slot->dump() << "]" << endl;
        dumpQueue();
    }
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::PlaybackQueue::peekSlot()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> slot(NULL);
    
    if (0 != this->size())
    {
        slot = *(this->begin());
    }
    
    return slot;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::popSlot()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> slot(NULL);
    
    if (0 != this->size())
    {
        slot = *(this->begin());
        LogTrace("playback-queue.log") << "▲pop [" << slot->dump() << "]" << endl;
        
        this->erase(this->begin());
        sort();
        
        dumpQueue();
    }
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackRate(double playbackRate)
{
    CriticalSectionScoped scopedCs(&accessCs_);
    playbackRate_ = playbackRate;
    
    LogTrace("playback-queue.log") << "update rate " << playbackRate << endl;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::clear()
{
    CriticalSectionScoped scopedCs(&accessCs_);
    PlaybackQueueBase::clear();
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::sort()
{
    std::sort(this->begin(), this->end(), comparator_);
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::dumpQueue()
{
    PlaybackQueueBase::iterator it;
    int i = 0;
    while (it != this->end())
    for (it = this->begin(); it != this->end(); ++it)
    {
        LogTrace("playback-queue.log") << "[" << setw(3) << i++ << ": "
        << (*it)->dump() << "]" << endl;
    }
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
ndnrtc::new_api::FrameBuffer::init()
{
    reset();
    
    {
        CriticalSectionScoped scopedCs(&syncCs_);
        initialize();
    }
    
    return RESULT_OK;
}

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
            slot->reset();
            freeSlots_.push_back(slot);
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
ndnrtc::new_api::FrameBuffer::interestIssued(ndn::Interest &interest)
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    shared_ptr<Slot> reservedSlot = getSlot(interest.getName(), false, true);
    
    // check if slot is already reserved
    if (!reservedSlot.get())
        reservedSlot = reserveSlot(interest);
    
    if (reservedSlot.get())
    {
      if (RESULT_GOOD(reservedSlot->addInterest(interest)))
      {
          isEstimationNeeded_ = true;
          return reservedSlot->getState();
      }
      else
          LogWarn(fetchChannel_->getLogFile())
          << "error adding " << interest.getName() << endl;
    }
    else
        LogWarn(fetchChannel_->getLogFile())
        << "no free slots" << endl;

    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::interestRangeIssued(const ndn::Interest &packetInterest,
                                                  SegmentNumber startSegmentNo,
                                                  SegmentNumber endSegmentNo,
                                                  std::vector<shared_ptr<Interest>> &segmentInterests)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(packetInterest.getName());
    
    if (packetNo < 0)
    {
        LogWarn(fetchChannel_->getLogFile())
        << "wrong prefix for range reservation - no packet number: " << packetInterest.getName() << endl;
        return Slot::StateFree;
    }
    
    CriticalSectionScoped scopedCs_(&syncCs_);
    shared_ptr<Slot> reservedSlot = getSlot(packetInterest.getName(), false, true);
    
    // check if slot is already reserved
    if (!reservedSlot.get())
        reservedSlot = reserveSlot(packetInterest);
    
    if (reservedSlot.get())
    {
        for (SegmentNumber segNo = startSegmentNo; segNo <= endSegmentNo; segNo++)
        {
            shared_ptr<Interest> segmentInterest(new Interest(packetInterest));
            segmentInterest->getName().appendSegment(segNo);
            
            if (RESULT_GOOD(reservedSlot->addInterest(*segmentInterest)))
            {
                segmentInterests.push_back(segmentInterest);
                
                LogTrace(fetchChannel_->getLogFile())
                << "pending " << segmentInterest->getName() << endl;
            }
            else
            {
                LogWarn(fetchChannel_->getLogFile())
                << "error adding " << segmentInterest->getName() << endl;
                break;
            }
        }
        
        isEstimationNeeded_ = true;
        
        return reservedSlot->getState();
    }
    else
        LogWarn(fetchChannel_->getLogFile())
        << "no free slots" << endl;
    
    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::newData(const ndn::Data &data)
{
    CriticalSectionScoped scopedCs(&syncCs_);
    const Name& dataName = data.getName();
    
    if (isWaitingForRightmost_)
        fixRightmost(dataName);
    
    shared_ptr<Slot> slot = getSlot(dataName, false, true);
    
    if (slot.get())
    {
        Slot::State oldState = slot->getState();
        int oldConsistency = slot->getConsistencyState();
        
        if (oldState == Slot::StateNew ||
            oldState == Slot::StateAssembling)
        {
            Slot::State newState = slot->appendData(data);
            int newConsistency = slot->getConsistencyState();
            
            LogTrace(fetchChannel_->getLogFile())
            << "appended " << dataName << " (" << slot->getAssembledLevel()*100 << "%)"
            << "with result " << Slot::stateToString(newState) << endl;
            
            if (oldState != newState ||
                newState == Slot::StateAssembling)
            {
                if (oldConsistency != newConsistency)
                {
                    if (newConsistency&Slot::HeaderMeta)
                    {
                        updateCurrentRate(slot->getPacketRate());
                        playbackQueue_.sort();
                    }
                    
                    playbackQueue_.updatePlaybackDeadlines();
                }
                
                // check for 1st segment
                if (oldState == Slot::StateNew)
                    addBufferEvent(Event::FirstSegment, slot);
                
                // check for ready event
                if (newState == Slot::StateReady)
                {
                    LogTrace(fetchChannel_->getLogFile())
                    << "ready " << slot->getPrefix() << endl;
                    
                    addBufferEvent(Event::Ready, slot);
                }
                
                return newState;
            }
        }
        else
            LogError(fetchChannel_->getLogFile())
            << "error appending " << data.getName()
            << " state: " << Slot::stateToString(oldState) << endl;
    }
    
    LogWarn(fetchChannel_->getLogFile())
    << "no reservation for " << data.getName()
    << " state " << FrameBuffer::stateToString(state_) << endl;

    return Slot::StateFree;
}

void
ndnrtc::new_api::FrameBuffer::interestTimeout(const ndn::Interest &interest)
{
    CriticalSectionScoped scopedCs(&syncCs_);
    const Name& prefix = interest.getName();
    
    shared_ptr<Slot> slot = getSlot(prefix, false, true);
    
    if (slot.get())
    {
        if (RESULT_GOOD(slot->markMissing(interest)))
        {
            LogTrace(fetchChannel_->getLogFile())
            << "timeout " << slot->getPrefix() << endl;
            
            addBufferEvent(Event::Timeout, slot);
        }
        else
        {
            LogTrace(fetchChannel_->getLogFile())
            << "timeout error " << interest.getName()
            << " for " << slot->getPrefix() << endl;
        }
    }
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::freeSlot(const ndn::Name &prefix)
{
    shared_ptr<Slot> slot = getSlot(prefix, true/*<=remove*/, true);
    
    if (slot.get())
    {
        if (slot->getState() != Slot::StateLocked)
        {
            slot->reset();
            freeSlots_.push_back(slot);
            
            addBufferEvent(Event::FreeSlot, slot);
            
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
ndnrtc::new_api::FrameBuffer::setState(const ndnrtc::new_api::FrameBuffer::State &state)
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    state_ = state;
    shared_ptr<Slot> nullSlot;
    addBufferEvent(Event::StateChanged, nullSlot);
}

void
ndnrtc::new_api::FrameBuffer::recycleOldSlots()
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    double playbackDuration = getEstimatedBufferSize();
    double targetSize = getTargetSize();
    int nRecycledSlots_ = 0;

    while (playbackDuration > getTargetSize()) {
        shared_ptr<Slot> oldSlot = playbackQueue_.peekSlot();
        playbackQueue_.popSlot();
        
        nRecycledSlots_++;
        freeSlot(oldSlot->getPrefix());
        playbackDuration = playbackQueue_.getPlaybackDuration();
    }
    
    isEstimationNeeded_ = true;
    
    LogTrace(fetchChannel_->getLogFile()) << "recycled "
        << nRecycledSlots_ << " slots" << endl;
}

int64_t
ndnrtc::new_api::FrameBuffer::getEstimatedBufferSize()
{
    if (isEstimationNeeded_)
    {
        CriticalSectionScoped scopedCs(&syncCs_);
        estimateBufferSize();
        isEstimationNeeded_ = false;
    }

    return estimatedSizeMs_;
}

int64_t
ndnrtc::new_api::FrameBuffer::getPlayableBufferSize()
{
    CriticalSectionScoped scopedCs(&syncCs_);
    return playbackQueue_.getPlaybackDuration(false);
}

void
ndnrtc::new_api::FrameBuffer::acquireSlot(ndnrtc::PacketData **packetData)
{
    CriticalSectionScoped scopedCs_(&syncCs_);

    playbackQueue_.sort();
    
    shared_ptr<FrameBuffer::Slot> slot = playbackQueue_.peekSlot();
    
    if (slot.get())
    {
        slot->lock();
        slot->getPacketData(packetData);
        
        addBufferEvent(Event::Playout, slot);
        
        LogTrace(fetchChannel_->getLogFile())
        << "locked " << slot->getPrefix() << " "
        << slot->getAssembledLevel()*100 << "%" << endl;
    }
    else
        LogWarn(fetchChannel_->getLogFile())
        << "no slot for playback" << endl;
}

int
ndnrtc::new_api::FrameBuffer::releaseAcquiredSlot()
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    
    int playbackDuration = 0;
    shared_ptr<FrameBuffer::Slot> lockedSlot = playbackQueue_.peekSlot();
    
    if (lockedSlot.get())
    {
        playbackQueue_.popSlot();
        
        shared_ptr<FrameBuffer::Slot> nextSlot = playbackQueue_.peekSlot();
        
        if (nextSlot.get() && nextSlot->getConsistencyState()&Slot::HeaderMeta)
        {
            playbackDuration = nextSlot->getProducerTimestamp() - lockedSlot->getProducerTimestamp();
            
            LogTrace(fetchChannel_->getLogFile())
            << "playback " << playbackDuration << endl;
        }
        else
        {
            playbackDuration = playbackQueue_.getInferredFrameDuration();
            
            LogTrace(fetchChannel_->getLogFile())
            << "playback (inferred)" << playbackDuration << endl;
        }
        
        LogTrace(fetchChannel_->getLogFile())
        << "unlocked " << lockedSlot->getPrefix() << endl;
        
        lockedSlot->unlock();
        freeSlot(lockedSlot->getPrefix());
        
        isEstimationNeeded_ = true;
    }
    
    return playbackDuration;
}

void
ndnrtc::new_api::FrameBuffer::recycleEvent(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    addBufferEvent(event.type_, event.slot_);
}

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::getSlot(const Name& prefix, bool remove,
                                      bool shouldMatch)
{
    shared_ptr<Slot> slot(NULL);
    Name lookupPrefix;
    
    if (getLookupPrefix(prefix, lookupPrefix))
    {
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
        activeSlots_[lookupPrefix] = slot;
        slot->setPrefix(lookupPrefix);
        
        dumpActiveSlots();
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void
ndnrtc::new_api::FrameBuffer::estimateBufferSize()
{
    playbackQueue_.sort();
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
                                             const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot)
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
    while (freeSlots_.size() < fetchChannel_->getParameters().bufferSize)
    {
        shared_ptr<Slot> slot(new Slot(fetchChannel_->getParameters().segmentSize));
        
        freeSlots_.push_back(slot);
        addBufferEvent(Event::FreeSlot, slot);
    }
    
    setTargetSize(fetchChannel_->getParameters().jitterSize);
    
    playbackQueue_.updatePlaybackRate(fetchChannel_->getParameters().producerRate);
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::reserveSlot(const ndn::Interest &interest)
{
    shared_ptr<Slot> reservedSlot(nullptr);
    
    if (freeSlots_.size() > 0)
    {
        reservedSlot = freeSlots_.back();
        setSlot(interest.getName(), reservedSlot);
        freeSlots_.pop_back();
        playbackQueue_.pushSlot(reservedSlot);
        
        LogTrace(fetchChannel_->getLogFile()) << "reserved "
        << reservedSlot->getPrefix() << endl;
    }
    
    return reservedSlot;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>>
ndnrtc::new_api::FrameBuffer::getSlots(int slotStateMask)
{
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
ndnrtc::new_api::FrameBuffer::fixRightmost(const Name& dataName)
{
    Name rightmostPrefix;
    NdnRtcNamespace::trimPacketNumber(dataName, rightmostPrefix);
 
    if (activeSlots_.find(rightmostPrefix) != activeSlots_.end())
    {
        shared_ptr<Slot> slot = activeSlots_[rightmostPrefix];
        activeSlots_.erase(rightmostPrefix);
        
        Name lookupPrefix;
        getLookupPrefix(dataName, lookupPrefix);
        
        activeSlots_[lookupPrefix] = slot;
        
        dumpActiveSlots();
        
        isWaitingForRightmost_ = false;
        
        LogTrace(fetchChannel_->getLogFile()) << "fixed righmost entry" << endl;
    }
}

void
ndnrtc::new_api::FrameBuffer::dumpActiveSlots()
{
    std::map<Name, shared_ptr<FrameBuffer::Slot>>::iterator it;
    int i = 0;
    
    LogTrace("buffer-dump.log") << "=== active slots dump" << endl;
    
    for (it = activeSlots_.begin(); it != activeSlots_.end(); ++it)
    {
        LogTrace("buffer-dump.log") << i << " "
        << it->first << " " << it->second.get() << endl;
        i++;
    }
}
