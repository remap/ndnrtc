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
