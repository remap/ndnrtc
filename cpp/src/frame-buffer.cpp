//
//  frame-buffer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "frame-buffer.hpp"

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/data.hpp>

#include "fec.hpp"
#include "clock.hpp"
#include "frame-data.hpp"
#include "name-components.hpp"
#include "simple-log.hpp"
#include "statistics.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

//******************************************************************************
SlotSegment::SlotSegment(const boost::shared_ptr<const ndn::Interest>& i):
interest_(i),
requestTimeUsec_(clock::microsecondTimestamp()),
arrivalTimeUsec_(0),
requestNo_(1),
isVerified_(false)
{
    if (!NameComponents::extractInfo(interest_->getName(), interestInfo_))
    {
        stringstream ss;
        ss << "Failed to create slot segment: wrong Interest name: " << interest_->getName();
        throw std::runtime_error(ss.str());
    }
}

const NamespaceInfo&
SlotSegment::getInfo() const
{
    if (isFetched()) return data_->getInfo();
    else return interestInfo_;
}

void
SlotSegment::setData(const boost::shared_ptr<WireSegment>& data) 
{ 
    arrivalTimeUsec_ = clock::microsecondTimestamp();
    data_ = data; 
}

bool
SlotSegment::isOriginal() const
{
    return data_->isOriginal();
}

int64_t
SlotSegment::getDrdUsec() const
{
    if (isOriginal())
        return getRoundTripDelayUsec() - (int64_t)(1000*data_->header().generationDelayMs_);
    return getRoundTripDelayUsec();
}

int64_t 
SlotSegment::getDgen() const
{
    return (int64_t)(data_->header().generationDelayMs_);
}

//******************************************************************************
static std::string
stateToString(BufferSlot::State s)
{
    switch (s) {
        case BufferSlot::Assembling: return "Assembling"; break;
        case BufferSlot::Free: return "Free"; break;
        case BufferSlot::Locked: return "Locked"; break;
        case BufferSlot::New: return "New"; break;
        case BufferSlot::Ready: return "Ready"; break;
        default: return "Unknown"; break;
    }
}

static std::string
toString(int consistency)
{
    std::stringstream str;
    if (consistency == BufferSlot::Consistent)
        str << "C";
    else if (consistency == BufferSlot::Inconsistent)
        str << "I";
    else
    {
        if (consistency&BufferSlot::SegmentMeta)
            str << "S";
        if (consistency&BufferSlot::HeaderMeta)
            str << "H";
    }
    return str.str();
}

BufferSlot::BufferSlot(){ clear(); }

void
BufferSlot::segmentsRequested(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests)
{
    if (state_ == Ready || state_ == Locked) 
        throw std::runtime_error("Can't add more segments because slot is ready or locked");

    for (auto i:interests)
    {
        boost::shared_ptr<SlotSegment> segment(boost::make_shared<SlotSegment>(i));
        
        if (!segment->getInfo().hasSeqNo_ || !segment->getInfo().hasSegNo_)
            throw std::runtime_error("No rightmost interests allowed: Interest should have segment-level info");
        
        if (name_.size() == 0) 
        {
            nameInfo_ = segment->getInfo();
            name_ = nameInfo_.getPrefix(prefix_filter::Sample);
            requestTimeUsec_ = segment->getRequestTimeUsec();
        }
        else if (!name_.match(i->getName()))
            throw std::runtime_error("Interest names should differ only after sample sequence number");

        Name segmentKey = segment->getInfo().getSuffix(suffix_filter::Segment);
        
        if (requested_.find(segmentKey) != requested_.end())
        {
            nRtx_++;
            requested_[segmentKey]->incrementRequestNum();
        }
        else requested_[segmentKey] = segment;

        if (state_ == Free) state_ = New;
    }
}

void 
BufferSlot::clear()
{
    name_.clear();
    nameInfo_ = NamespaceInfo();
    requested_.clear();
    fetched_.clear();
    consistency_ = Inconsistent;
    requestTimeUsec_ = 0;
    assembledSize_ = 0;
    asmLevel_ = 0;
    assembledTimeUsec_ = 0;
    firstSegmentTimeUsec_ = 0;
    requestTimeUsec_ = 0;
    hasOriginalSegments_ = false;
    assembled_ = 0.;
    nRtx_ = 0;
    state_ = Free;
    lastFetched_.reset();
    nDataSegments_ = 0;
    nParitySegments_ = 0;
    verified_ = Verification::Unknown;
    manifest_.reset();
}

const boost::shared_ptr<SlotSegment>
BufferSlot::segmentReceived(const boost::shared_ptr<WireSegment>& segment)
{
    assert(segment->isValid());

    if (state_ == Locked)
        return boost::shared_ptr<SlotSegment>();
    
    if (!name_.match(segment->getData()->getName()))
        throw std::runtime_error("Attempt to add data segment with incorrect name");

    Name segmentKey = segment->getInfo().getSuffix(suffix_filter::Segment);

    if (!segment->getInfo().hasSeqNo_ || 
        requested_.find(segmentKey) == requested_.end())
        throw std::runtime_error("Adding segment that was not previously requested");
    
    if (fetched_.find(segmentKey) == fetched_.end())
    {
        lastFetched_ = fetched_[segmentKey] = requested_[segmentKey];
        fetched_[segmentKey]->setData(segment);
        updateConsistencyState(fetched_[segmentKey]);
    }

    return fetched_[segmentKey];
}

std::vector<ndn::Name>
BufferSlot::getMissingSegments() const
{
    std::vector<ndn::Name> missing;
    
    if (getFetchedNum() > 0)
    {
        for (unsigned int segNo = 0; segNo < nDataSegments_; ++segNo)
        {
            Name segKey = Name().appendSegment(segNo);
            if (requested_.find(segKey) == requested_.end())
                missing.push_back(Name(getPrefix()).append(segKey));
        }
        for (unsigned int segNo = 0; segNo < nParitySegments_; ++segNo)
        {
            Name segKey = Name(NameComponents::NameComponentParity).appendSegment(segNo);
            if (requested_.find(segKey) == requested_.end())
                missing.push_back(Name(getPrefix()).append(segKey));
        }
    }
    
    return missing;
}

const std::vector<boost::shared_ptr<const ndn::Interest>>
BufferSlot::getPendingInterests() const
{
    std::vector<boost::shared_ptr<const ndn::Interest>> pendingInterests;

    for (auto it:requested_)
        if (fetched_.find(it.first) == fetched_.end())
            pendingInterests.push_back(it.second->getInterest());

    return pendingInterests;
}

const std::vector<boost::shared_ptr<const SlotSegment>>
BufferSlot::getFetchedSegments() const
{
    std::vector<boost::shared_ptr<const SlotSegment>> segments;
    
    for (auto it:fetched_)
        segments.push_back(it.second);

    return segments;
}

int 
BufferSlot::getRtxNum(const ndn::Name& segmentName)
{
    NamespaceInfo info;
    if (NameComponents::extractInfo(segmentName, info))
    {
        Name segmentKey = info.getSuffix(suffix_filter::Segment);
        if (requested_.find(segmentKey) != requested_.end())
           return requested_[segmentKey]->getRequestNum()-1;
    }

    return -1;
}

std::string
BufferSlot::dump(bool showLastSegment) const
{
    std::stringstream dump;

    dump << "[ "
    // << (nameInfo_.class_ == SampleClass::Delta?"D":((nameInfo_.class_ == SampleClass::Key)?"K":"U")) << ", "
    << std::setw(5) 
    << nameInfo_.sampleNo_
    << (verified_ == Verification::Verified ? "☆" : (verified_ == Verification::Unknown? "?" : "✕")) << ", "
    << toString(getConsistencyState()) << ", "
    // << std::setw(7) << getPlaybackNumber() << ", "
    // << std::setw(10) << getProducerTimestamp() << ", "
    << std::setw(3) << asmLevel_*100 << "% "//"("
    // << ((double)nSegmentsParityReady_/(double)nSegmentsParity_)*100 << "%), "
    // << std::setw(5) << getPairedFrameNumber() << ", "
    // << std::setw(3) << getPlaybackDeadline() << ", "
    << std::setw(3) << nRtx_ << ", "
    // << (isRecovered_ ? "R" : "I") << ", "
    // << std::setw(2) << nSegmentsTotal_ << "/" << nSegmentsReady_
    // << "/" << nSegmentsPending_ << "/" << nSegmentsMissing_
    // << "/" << nSegmentsParity_ << " "
    // << getLifetime() << " "
    << std::setw(5) << assembledSize_ << " "
    << (showLastSegment ? lastFetched_->getInfo().getSuffix(suffix_filter::Thread) : nameInfo_.getSuffix(suffix_filter::Thread))
    << " " << (lastFetched_ && lastFetched_->isOriginal() ? "ORIG" : "CACH")
    << " dgen " << (showLastSegment ? lastFetched_->getDgen() : -1)
    << " rtt " << (showLastSegment ? lastFetched_->getRoundTripDelayUsec()/1000 : -1)
    << " " << std::setw(5) << (getConsistencyState() & BufferSlot::HeaderMeta ? getHeader().publishTimestampMs_ : 0)
    << "]";

    return dump.str();
}

const CommonHeader
BufferSlot::getHeader() const
{
    if (!consistency_&HeaderMeta)
        throw std::runtime_error("Packet header is not available");

    return fetched_.at(nameInfo_.getSuffix(suffix_filter::Segment))->getData()->packetHeader();
}

void
BufferSlot::updateConsistencyState(const boost::shared_ptr<SlotSegment>& segment)
{
    if (state_ == New)
    {
        nameInfo_ = segment->getData()->getInfo();
        nameInfo_.segNo_ = 0; // make sure it always points to segment 0
        firstSegmentTimeUsec_ = segment->getArrivalTimeUsec();
    }

    if (segment->getInfo().segmentClass_ == SegmentClass::Data)
    {
        consistency_ |= SegmentMeta;
        nDataSegments_ = segment->getData()->getSlicesNum();
        if (segment->getInfo().segNo_ == 0)
            consistency_ |= HeaderMeta;
    }
    else if (segment->getInfo().segmentClass_ == SegmentClass::Parity)
        nParitySegments_ = segment->getData()->getSlicesNum();

    assembledSize_ += segment->getData()->getData()->getContent().size();
    hasOriginalSegments_ = segment->isOriginal();
    assembled_ += segment->getData()->getSegmentWeight();
    
    if (consistency_&SegmentMeta)
    {
        updateAssembledLevel();
        state_ = (assembled_ >= nDataSegments_ ? Ready : Assembling);
    }

    if (state_ == Ready) assembledTimeUsec_ = segment->getArrivalTimeUsec();
}

void BufferSlot::updateAssembledLevel()
{
    if (consistency_&SegmentMeta)
    {
        asmLevel_ = 0;
        for (auto& it:fetched_) asmLevel_ += it.second->getData()->getShareSize(nDataSegments_);
    }
}

void
BufferSlot::toggleLock()
{
    if (state_ == BufferSlot::Locked)
        state_ = BufferSlot::Ready;
    else
    {
        if (state_ != BufferSlot::State::Ready)
            throw std::runtime_error("Trying to lock slot whith is not ready");
        state_ = BufferSlot::State::Locked;
    }
}

//******************************************************************************
VideoFrameSlot::VideoFrameSlot(const size_t storageSize):
storage_(boost::make_shared<std::vector<uint8_t>>())
{
    storage_->reserve(storageSize);
    fecList_.reserve(1000);
}

boost::shared_ptr<ImmutableVideoFramePacket>
VideoFrameSlot::readPacket(const BufferSlot& slot, bool& recovered)
{
    if (slot.getNameInfo().streamType_ != 
        MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    // check if recovery is possible
    Name parityKey(NameComponents::NameComponentParity);
    std::map<ndn::Name, boost::shared_ptr<SlotSegment>> 
        dataSegments(slot.fetched_.begin(), slot.fetched_.lower_bound(parityKey));
    std::map<ndn::Name, boost::shared_ptr<SlotSegment>> 
        paritySegments(slot.fetched_.upper_bound(parityKey), slot.fetched_.end());

    if ((!paritySegments.size() && 
        dataSegments.size() < dataSegments.begin()->second->getData()->getSlicesNum()) ||
        dataSegments.size() == 0)
    {
        recovered = false;
        return boost::shared_ptr<ImmutableVideoFramePacket>();
    }

    boost::shared_ptr<WireData<VideoFrameSegmentHeader>> firstSeg = 
            boost::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(dataSegments.begin()->second->getData());
    boost::shared_ptr<WireData<VideoFrameSegmentHeader>> firstParitySeg;

    if (paritySegments.size()) 
        firstParitySeg = boost::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(paritySegments.begin()->second->getData());

    size_t segmentSize = firstSeg->segment().getPayload().size();
    size_t paritySegSize = (paritySegments.size() ? firstParitySeg->segment().getPayload().size() : 0);
    unsigned int nDataSegmentsExpected = firstSeg->getSlicesNum();
    unsigned int nParitySegmentsExpected = (paritySegments.size() ? paritySegments.begin()->second->getData()->getSlicesNum() : 0);

    fecList_.assign(nDataSegmentsExpected+nParitySegmentsExpected, FEC_RLIST_SYMEMPTY);
    storage_->resize(segmentSize*(nDataSegmentsExpected+nParitySegmentsExpected));

    int segNo = 0;
    for (auto it:dataSegments)
    {
        const boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd = 
            boost::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(it.second->getData());
        
        while (segNo != wd->getSegNo() && segNo < nDataSegmentsExpected)
            storage_->insert(storage_->begin()+segmentSize*segNo++, segmentSize, 0);
        
        if (segNo < nDataSegmentsExpected)
        {
            storage_->insert(storage_->begin()+segmentSize*segNo, 
                wd->segment().getPayload().begin(),
                wd->segment().getPayload().end());
            fecList_[segNo] = FEC_RLIST_SYMREADY;
            segNo++;
        }
    }

    bool frameExtracted = false;
    if (dataSegments.size() < nDataSegmentsExpected)
    {
        segNo = 0;
        for (auto it:paritySegments)
        {
            const boost::shared_ptr<WireData<VideoFrameSegmentHeader>> wd =
            boost::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(it.second->getData());

            while (segNo != wd->getSegNo() && segNo < nParitySegmentsExpected)
                storage_->insert(storage_->begin()+nDataSegmentsExpected*segmentSize + paritySegSize*segNo++, segmentSize, 0);

            if (segNo < nParitySegmentsExpected)
            {
                storage_->insert(storage_->begin()+nDataSegmentsExpected*segmentSize+paritySegSize*segNo, 
                    wd->segment().getPayload().begin(),
                    wd->segment().getPayload().end());            

                fecList_[nDataSegmentsExpected+segNo] = FEC_RLIST_SYMREADY;
                segNo++;
            }
        }
        
        fec::Rs28Decoder dec(nDataSegmentsExpected, nParitySegmentsExpected, segmentSize);
        int nRecovered = dec.decode(storage_->data(),
            storage_->data()+nDataSegmentsExpected*segmentSize,
            fecList_.data());
        recovered = (nRecovered+dataSegments.size() >= nDataSegmentsExpected);
        frameExtracted = recovered;
    }
    else 
        frameExtracted = true;

    storage_->resize(nDataSegmentsExpected*segmentSize);

    return (frameExtracted ? boost::make_shared<ImmutableVideoFramePacket>(storage_) : 
                boost::shared_ptr<ImmutableVideoFramePacket>());
}

VideoFrameSegmentHeader
VideoFrameSlot::readSegmentHeader(const BufferSlot& slot)
{
    if (slot.getNameInfo().streamType_ != 
        MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    std::map<ndn::Name, boost::shared_ptr<SlotSegment>> 
        dataSegments(slot.fetched_.begin(), slot.fetched_.lower_bound(Name(NameComponents::NameComponentParity)));

    if (!dataSegments.size())
        return VideoFrameSegmentHeader();

    boost::shared_ptr<WireData<VideoFrameSegmentHeader>> seg = 
            boost::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(dataSegments.begin()->second->getData());

    return seg->segment().getHeader();
}

//******************************************************************************
AudioBundleSlot::AudioBundleSlot(const size_t storageSize):
storage_(boost::make_shared<std::vector<uint8_t>>())
{
    storage_->reserve(storageSize);
}

boost::shared_ptr<ImmutableAudioBundlePacket> 
AudioBundleSlot::readBundle(const BufferSlot& slot)
{
     if (slot.getNameInfo().streamType_ != 
        MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    if (slot.getAssembledLevel() < 1.)
        return boost::shared_ptr<ImmutableAudioBundlePacket>();

    boost::shared_ptr<WireData<DataSegmentHeader>> firstSeg = 
            boost::dynamic_pointer_cast<WireData<DataSegmentHeader>>(slot.fetched_.begin()->second->getData());
    size_t segmentSize = firstSeg->segment().getPayload().size();
    unsigned int nDataSegmentsExpected = firstSeg->getSlicesNum();

    storage_->resize(segmentSize*nDataSegmentsExpected);

    for (auto it:slot.fetched_)
    {
        const boost::shared_ptr<WireData<DataSegmentHeader>> wd = 
            boost::dynamic_pointer_cast<WireData<DataSegmentHeader>>(it.second->getData());
        storage_->insert(storage_->begin(), 
                wd->segment().getPayload().begin(),
                wd->segment().getPayload().end());
    }

    return boost::make_shared<ImmutableAudioBundlePacket>(storage_);
}

//******************************************************************************
SlotPool::SlotPool(const size_t& capacity):
capacity_(capacity)
{   
    for (int i = 0; i < capacity; ++i)
        pool_.push_back(boost::make_shared<BufferSlot>());
}

boost::shared_ptr<BufferSlot>
SlotPool::pop()
{
    if (pool_.size())
    {
        boost::shared_ptr<BufferSlot> slot = pool_.back();
        pool_.pop_back();
        return slot;
    }
    return boost::shared_ptr<BufferSlot>();
}

bool
SlotPool::push(const boost::shared_ptr<BufferSlot>& slot)
{
    if (pool_.size() < capacity_)
    {
        slot->clear();
        pool_.push_back(slot);
        return true;
    }

    return false;
}

//******************************************************************************
Buffer::Buffer(boost::shared_ptr<StatisticsStorage> storage,
               boost::shared_ptr<SlotPool> pool):pool_(pool),
sstorage_(storage)
{
    assert(sstorage_.get());
    description_ = "buffer";
}

void
Buffer::reset()
{   
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    
    for (auto s:activeSlots_)
        pool_->push(s.second);
    activeSlots_.clear();
 
     LogDebugC << "slot pool capacity " << pool_->capacity()
        << " pool size " << pool_->size() << " "
        << reservedSlots_.size() << " slot(s) locked for playback" << std::endl;

    for (auto o:observers_) o->onReset();
}

bool
Buffer::requested(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests)
{
    std::map<Name, std::vector<boost::shared_ptr<const Interest>>> slotInterests;
    for (auto i:interests)
    {
        NamespaceInfo nameInfo;

        if (!NameComponents::extractInfo(i->getName(), nameInfo)) 
        {
            stringstream ss;
            ss << "Incorrect Interest name supplied: " << i->getName();
            throw std::runtime_error(ss.str());
        }

        slotInterests[nameInfo.getPrefix(prefix_filter::Sample)].push_back(i);
    }

    for (auto it:slotInterests)
    {
        bool newRequest = false;
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        if (activeSlots_.find(it.first) == activeSlots_.end())
        {
            if (pool_->size() == 0)
            {
                LogErrorC << "no free slots available" << std::endl;
                return false;
            }
            else
            {
                activeSlots_[it.first] = pool_->pop();
                newRequest = true;
            }
        }
        
        activeSlots_[it.first]->segmentsRequested(it.second);
        
        if (newRequest) 
            for (auto o:observers_) o->onNewRequest(activeSlots_[it.first]);

        LogTraceC << "▷▷▷" << activeSlots_[it.first]->dump()
        << " x" << it.second.size() << std::endl;
        //LogDebugC << shortdump() << std::endl;
        LogTraceC << dump() << std::endl;
    }

    return true;
}

BufferReceipt
Buffer::received(const boost::shared_ptr<WireSegment>& segment)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    
    BufferReceipt receipt;
    Name key = segment->getInfo().getPrefix(prefix_filter::Sample);
    
    if (activeSlots_.find(key) == activeSlots_.end())
    {
        stringstream ss;
        ss << "Received data that was not previously requested: "
        << key;
        throw std::runtime_error(ss.str());
    }
    
    BufferSlot::State oldState = activeSlots_[key]->getState();
    receipt.segment_ = activeSlots_[key]->segmentReceived(segment);
    receipt.slot_ = activeSlots_[key];
    receipt.oldState_ = oldState;
    
    if (receipt.slot_->getState() == BufferSlot::Ready)
    {
        if (oldState != BufferSlot::Ready)
        {
            LogTraceC << "►►►" << receipt.slot_->dump(true)
                << " " << shortdump() << std::endl;
            
            (*sstorage_)[Indicator::AssembledNum]++;
            if (receipt.slot_->getNameInfo().class_ == SampleClass::Key)
            {
                (*sstorage_)[Indicator::AssembledKeyNum]++;
                (*sstorage_)[Indicator::FrameFetchAvgKey] = (double)receipt.slot_->getLongestDrd()/1000.;
            }
            else
            {
                (*sstorage_)[Indicator::FrameFetchAvgDelta] = (double)receipt.slot_->getLongestDrd()/1000.;
            }
            
        }
    }
    else
    {
        if (receipt.oldState_ == BufferSlot::New)
            LogDebugC << "new sample " 
                      << receipt.segment_->getInfo().getSuffix(suffix_filter::Thread) 
                      << std::endl;
        LogTraceC << " ► " << receipt.slot_->dump(true)
                  << receipt.segment_->getInfo().segNo_ << std::endl;
    }
    
    for (auto o:observers_) o->onNewData(receipt);
    
    return receipt;
}

bool
Buffer::isRequested(const boost::shared_ptr<WireSegment>& segment) const
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    Name key = segment->getInfo().getPrefix(prefix_filter::Sample);

    return (activeSlots_.find(key) != activeSlots_.end());
}

unsigned int 
Buffer::getSlotsNum(const ndn::Name& prefix, int stateMask) const
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    unsigned int nSlots = 0;

    for (auto it:activeSlots_)
        if (prefix.match(it.first) && it.second->getState()&stateMask)
            nSlots++;

    return nSlots;
}

void
Buffer::attach(IBufferObserver* observer)
{
    if (observer)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        observers_.push_back(observer);
    }
}

void
Buffer::detach(IBufferObserver* observer)
{
    std::vector<IBufferObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end())
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}

void
Buffer::invalidate(const Name& slotPrefix)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    assert(activeSlots_.find(slotPrefix) != activeSlots_.end());

    boost::shared_ptr<BufferSlot> slot = activeSlots_.find(slotPrefix)->second;
    activeSlots_.erase(activeSlots_.find(slotPrefix));
    
    (*sstorage_)[Indicator::DroppedNum]++;
    if (slot->getState() <= BufferSlot::Assembling)
        (*sstorage_)[Indicator::IncompleteNum]++;
    if (slot->getNameInfo().class_ == SampleClass::Key)
    {
        (*sstorage_)[Indicator::DroppedKeyNum]++;
        if (slot->getState() <= BufferSlot::Assembling)
            (*sstorage_)[Indicator::IncompleteKeyNum]++;
    }
    
    pool_->push(slot);
}

void
Buffer::invalidatePrevious(const Name& slotPrefix)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, boost::shared_ptr<BufferSlot>>::iterator lower = 
        activeSlots_.lower_bound(slotPrefix);

    while (activeSlots_.begin() != lower)
    {
        LogDebugC << "invalidate " << activeSlots_.begin()->first << std::endl;
        boost::shared_ptr<BufferSlot> slot = activeSlots_.begin()->second;
        
        (*sstorage_)[Indicator::DroppedNum]++;
        if (slot->getState() <= BufferSlot::Assembling)
            (*sstorage_)[Indicator::IncompleteNum]++;
        if (slot->getNameInfo().class_ == SampleClass::Key)
        {
            (*sstorage_)[Indicator::DroppedKeyNum]++;
            if (slot->getState() <= BufferSlot::Assembling)
                (*sstorage_)[Indicator::IncompleteKeyNum]++;
        }
        
        pool_->push(activeSlots_.begin()->second);
        activeSlots_.erase(activeSlots_.begin());
    }
}

#if 0
void
Buffer::invalidateOldKey(const Name& slotPrefix)
{
    
}
#endif

void
Buffer::reserveSlot(const boost::shared_ptr<const BufferSlot>& slot)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, boost::shared_ptr<BufferSlot>>::iterator it =
    activeSlots_.find(slot->getPrefix());
    
    if (it != activeSlots_.end())
    {
        reservedSlots_[slot->getPrefix()] = activeSlots_[slot->getPrefix()];
        activeSlots_.erase(it);
        reservedSlots_[slot->getPrefix()]->toggleLock();
    }
}

void
Buffer::releaseSlot(const boost::shared_ptr<const BufferSlot>& slot)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, boost::shared_ptr<BufferSlot>>::iterator it =
    reservedSlots_.find(slot->getPrefix());
    
    if (it != reservedSlots_.end())
    {
        pool_->push(it->second);
        reservedSlots_.erase(it);
    }
}

std::string
Buffer::dump() const
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    int i = 0;
    stringstream ss;
    ss << "buffer dump:";

    for (auto& s:activeSlots_)
        ss << std::endl << ++i << " " << s.second->dump();

    return ss.str();
}

std::string
Buffer::shortdump() const
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    stringstream ss;
    ss << "[ ";

    //dumpSlotDictionary(ss, reservedSlots_);
    dumpSlotDictionary(ss, activeSlots_);

    ss << " ]";

    return ss.str();
}

void
Buffer::dumpSlotDictionary(stringstream& ss, 
    const map<ndn::Name, boost::shared_ptr<BufferSlot>> &slotDict) const
{
    int i = 0;
    for (auto& s:slotDict)
    {
        if ((i++ % 10 == 0) || !s.second->getNameInfo().isDelta_ )
        {
            ss << s.second->getNameInfo().sampleNo_; 
            ss << (s.second->getNameInfo().isDelta_ ? "" : "K");
        }

        ss << (s.second->getAssembledLevel() >= 1 ? "■" :
            (s.second->getAssembledLevel() > 0 ? "◘" : "☐" ));
    }
}

//******************************************************************************
int64_t
PlaybackQueue::Sample::timestamp() const
{ 
    return slot_->getHeader().publishTimestampMs_; 
}

PlaybackQueue::PlaybackQueue(const ndn::Name& streamPrefix, 
    const boost::shared_ptr<Buffer>& buffer):
streamPrefix_(streamPrefix),
buffer_(buffer),
packetRate_(0),
sstorage_(buffer->sstorage_)
{
    description_ = "pqueue";
    buffer_->attach(this);
}

PlaybackQueue::~PlaybackQueue()
{
    buffer_->detach(this);
}

void
PlaybackQueue::pop(ExtractSlot extract)
{
    if (queue_.size())
    {
        boost::shared_ptr<const BufferSlot> slot;
        
        { 
            boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);

            slot = queue_.begin()->slot();
            queue_.erase(queue_.begin());
        }

        double playTime = (queue_.size() ? queue_.begin()->timestamp() - slot->getHeader().publishTimestampMs_ : samplePeriod());
        
        LogTraceC << "-■-" << slot->dump()  << "~" << (int)playTime << "ms " 
            << dump() << std::endl;

        extract(slot, playTime);
        (*sstorage_)[Indicator::AcquiredNum]++;
        
        if (slot->getNameInfo().isDelta_)
            buffer_->invalidatePrevious(slot->getPrefix());
        else
        {
            // TODO: invalidate old key frames
            (*sstorage_)[Indicator::AcquiredKeyNum]++;
        }
        
        buffer_->releaseSlot(slot);
    }
}

int64_t
PlaybackQueue::size() const
{
    if (!queue_.size()) return 0;
    if (queue_.size() == 1) return samplePeriod();
    return (--queue_.end())->timestamp() - queue_.begin()->timestamp() + samplePeriod();
}

int64_t
PlaybackQueue::pendingSize() const
{
    return (packetRate_ > 0 ? 1000./packetRate_ * buffer_->getSlotsNum(streamPrefix_, BufferSlot::New|BufferSlot::Assembling) :
        0.);
}

void 
PlaybackQueue::attach(IPlaybackQueueObserver* observer)
{
    if (observer)
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        observers_.push_back(observer);
    }
}

void
PlaybackQueue::detach(IPlaybackQueueObserver* observer)
{
    std::vector<IPlaybackQueueObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end())
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}

std::string
PlaybackQueue::dump()
{
    std::stringstream ss;
    ss.precision(2);
    
    ss << "[ ";
    int idx = 0;
    for (auto s:queue_)
    {
        if ((idx++ % 10 == 0) || //s.slot()->getNameInfo().sampleNo_%10 == 0 ||
            !s.slot()->getNameInfo().isDelta_)
            ss  << s.slot()->getNameInfo().sampleNo_;
        if (s.slot()->getVerificationStatus() == BufferSlot::Verification::Verified)
            ss << (s.slot()->getNameInfo().isDelta_ ? "D" : "K");
        else
            ss << (s.slot()->getNameInfo().isDelta_ ? "d" : "k");
    }
    ss << "]" << size() << "ms";
    
    return ss.str();
}

// IBufferObserver
void 
PlaybackQueue::onNewRequest(const boost::shared_ptr<BufferSlot>&)
{
    (*sstorage_)[Indicator::BufferReservedSize] = pendingSize();
}

void 
PlaybackQueue::onNewData(const BufferReceipt& receipt)
{
    if (receipt.slot_->getState() == BufferSlot::Ready &&
        streamPrefix_.match(receipt.slot_->getPrefix()))
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        buffer_->reserveSlot(receipt.slot_);
        packetRate_ = receipt.slot_->getHeader().sampleRate_;
        queue_.insert(Sample(receipt.slot_));

        for (auto o:observers_) o->onNewSampleReady();
        
        LogDebugC << "--■ add assembled frame " << receipt.slot_->dump() << std::endl;
        LogDebugC << "dump " << dump() << buffer_->shortdump() << std::endl;
        
        (*sstorage_)[Indicator::BufferPlayableSize] = size();
    }
}

void
PlaybackQueue::onReset()
{
//    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
//    queue_.clear();
}
