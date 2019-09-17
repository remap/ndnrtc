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

#include <thread>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/data.hpp>

#include "async.hpp"
#include "clock.hpp"
#include "fec.hpp"
#include "frame-data.hpp"
#include "interest-queue.hpp"
#include "name-components.hpp"
#include "proto/ndnrtc.pb.h"
#include "packets.hpp"
#include "simple-log.hpp"
#include "statistics.hpp"

using namespace std;
using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

//******************************************************************************
SlotSegment::SlotSegment(const std::shared_ptr<const ndn::Interest>& i):
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
SlotSegment::setData(const std::shared_ptr<WireSegment>& data)
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

SlotTriggerConnection
BufferSlot::subscribe(PipelineSlotState slotState, OnSlotStateUpdate onSlotStateUpdate)
{
    SlotTriggerConnection c;

    switch (slotState) {
        case PipelineSlotState::Pending: c = onPending_.connect(onSlotStateUpdate); break;
        case PipelineSlotState::Assembling: c = onAssembling_.connect(onSlotStateUpdate); break;
        case PipelineSlotState::Ready: c = onReady_.connect(onSlotStateUpdate); break;
        case PipelineSlotState::Unfetchable: c = onUnfetchable_.connect(onSlotStateUpdate); break;
        default:
            throw runtime_error("This slot state update is not supported");
            break;
    }

    return c;
}

NeedDataTriggerConnection
BufferSlot::addOnNeedData(OnNeedData onNeedData)
{
    return onMissing_.connect(onNeedData);
}

void
BufferSlot::setRequests(const std::vector<std::shared_ptr<DataRequest> > &requests)
{
    if (slotState_ == PipelineSlotState::Ready)
        throw std::runtime_error("Can't add more segments because slot is ready");

    for (auto& r:requests)
    {
        if (name_.size() == 0)
        {
            nameInfo_ = r->getNamespaceInfo();
            name_ = nameInfo_.getPrefix(NameFilter::Sample);
            nameInfo_.segmentClass_ = SegmentClass::Unknown;

            if (slotState_ == PipelineSlotState::Free)
                slotState_ = PipelineSlotState::New;
        }

        if (r->getNamespaceInfo().segmentClass_ == SegmentClass::Data &&
            r->getNamespaceInfo().segNo_ > maxDataSegNo_)
            maxDataSegNo_ = r->getNamespaceInfo().segNo_;

        if (r->getNamespaceInfo().segmentClass_ == SegmentClass::Parity &&
            r->getNamespaceInfo().segNo_ > maxParitySegNo_)
            maxParitySegNo_ = r->getNamespaceInfo().segNo_;

        std::shared_ptr<BufferSlot> me = shared_from_this();
        {
            auto c =
                r->subscribe(DataRequest::Status::Expressed, [this, me](const DataRequest& dr){
                    if (firstRequestTsUsec_ == 0)
                        firstRequestTsUsec_ = dr.getRequestTimestampUsec();
                    if (slotState_ == PipelineSlotState::New)
                    {
                        slotState_ = PipelineSlotState::Pending;
                        triggerEvent(PipelineSlotState::Pending, dr);
                    }
                });
            requestConnections_.push_back(c);
        }
        {
            auto c = r->subscribe(DataRequest::Status::Data, std::bind(&BufferSlot::onReply, me, _1));
            requestConnections_.push_back(c);
        }
        {
            auto c = r->subscribe(DataRequest::Status::Timeout, std::bind(&BufferSlot::onError, me, _1));
            requestConnections_.push_back(c);
        }
        {
            auto c = r->subscribe(DataRequest::Status::AppNack, std::bind(&BufferSlot::onError, me, _1));
            requestConnections_.push_back(c);
        }
        {
            auto c = r->subscribe(DataRequest::Status::NetworkNack, std::bind(&BufferSlot::onError, me, _1));
            requestConnections_.push_back(c);
        }

        requests_.push_back(r);
    }
}

const vector<std::shared_ptr<DataRequest>>&
BufferSlot::getRequests() const
{
    return requests_;
}

void
BufferSlot::clear()
{
    name_.clear();
    slotState_ = PipelineSlotState::Free;
    nameInfo_ = NamespaceInfo();
    requests_.clear();
    for (auto &c:requestConnections_) c.disconnect();
    requestConnections_.clear();
    onReady_.disconnect_all_slots();
    onUnfetchable_.disconnect_all_slots();
    onMissing_.disconnect_all_slots();
    metaIsFetched_ = manifestIsFetched_ = false;
    meta_.reset();
    manifest_.reset();
    maxDataSegNo_ = 0;
    maxParitySegNo_ = 0;
    firstRequestTsUsec_ = firstDataTsUsec_ = lastDataTsUsec_ = 0;
    nDataSegments_ = nParitySegments_ = 0;
    nDataSegmentsFetched_ = nParitySegmentsFetched_ = 0;
    fetchedBytesData_ = fetchedBytesParity_ = fetchedBytesTotal_ = 0;
    fetchProgress_ = 0;

    // code below is deprecated
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
//    manifest_.reset();
}

bool
BufferSlot::isReadyForDecoder() const
{
    return nDataSegments_ > 0 &&
           (nDataSegmentsFetched_ + nParitySegmentsFetched_/2 >= nDataSegments_);
}

// private
void
BufferSlot::onReply(const DataRequest& dr)
{
    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Meta &&
        !metaIsFetched_)
    {
        metaIsFetched_ = true;
        meta_ = std::dynamic_pointer_cast<const packets::Meta>(dr.getNdnrtcPacket());
        checkForMissingSegments(dr);
    }

    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Manifest &&
        !manifestIsFetched_)
    {
        manifestIsFetched_ = true;
        manifest_ = std::dynamic_pointer_cast<const packets::Manifest>(dr.getNdnrtcPacket());
        // TODO -- verification
    }

    if (!metaIsFetched_ &&
        (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data ||
         dr.getNamespaceInfo().segmentClass_ == SegmentClass::Parity))
        checkForMissingSegments(dr);

    updateAssemblingProgress(dr);
}

void
BufferSlot::onError(const DataRequest& dr)
{
    assert(dr.getNamespaceInfo().getPrefix(NameFilter::Sample) == getPrefix());

    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Meta ||
        (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data && dr.getNamespaceInfo().segNo_ < nDataSegments_))
    {
        assert(slotState_ != PipelineSlotState::Ready);
        if (slotState_ != PipelineSlotState::Unfetchable)
        {
            slotState_ = PipelineSlotState::Unfetchable;
            triggerEvent(PipelineSlotState::Unfetchable, dr);
        }
    }
}

void
BufferSlot::checkForMissingSegments(const DataRequest& dr)
{
    int nParityMissing = 0;
    int nDataMissing = 0;

    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Meta)
    {
        std::shared_ptr<const packets::Meta> meta = meta_;

        nParityMissing = meta->getFrameMeta().parity_size() - maxParitySegNo_ - 1;
        nDataMissing = meta->getFrameMeta().dataseg_num() - maxDataSegNo_ - 1;
    }

    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data ||
        dr.getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
    {
        std::shared_ptr<const packets::Segment> seg =
            std::dynamic_pointer_cast<const packets::Segment>(dr.getNdnrtcPacket());

        if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
            nParityMissing  = (int)seg->getTotalSegmentsNum() - maxParitySegNo_ - 1;
        if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data)
            nDataMissing  = (int)seg->getTotalSegmentsNum() - maxDataSegNo_ - 1;
    }

    vector<std::shared_ptr<DataRequest>> requests;
    if (nParityMissing > 0)
        for (int seg = maxParitySegNo_+1; seg <= maxParitySegNo_+nParityMissing; ++seg)
        {
            std::shared_ptr<Interest> i = std::make_shared<Interest>(*dr.getInterest());
            i->setName(Name(name_).append(NameComponents::Parity).appendSegment(seg));

            std::shared_ptr<DataRequest> r = std::make_shared<DataRequest>(i);
            requests.push_back(r);
        }

    if (nDataMissing > 0)
        for (int seg = maxDataSegNo_+1; seg <= maxDataSegNo_+nDataMissing; ++seg){
            std::shared_ptr<Interest> i = std::make_shared<Interest>(*dr.getInterest());
            i->setName(Name(name_).appendSegment(seg));

            std::shared_ptr<DataRequest> r = std::make_shared<DataRequest>(i);
            requests.push_back(r);
        }

    if (requests.size())
        onMissing_(this, requests);
}

void
BufferSlot::updateAssemblingProgress(const DataRequest &dr)
{
    if (slotState_ == PipelineSlotState::Pending)
        firstDataTsUsec_ = dr.getReplyTimestampUsec();

    lastDataTsUsec_ = dr.getReplyTimestampUsec();

    if (nDataSegments_ == 0)
    {
        if (meta_)
            nDataSegments_ = meta_->getFrameMeta().dataseg_num();
        else if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data)
        {
            std::shared_ptr<const packets::Segment> seg =
                std::dynamic_pointer_cast<const packets::Segment>(dr.getNdnrtcPacket());
            nDataSegments_ = seg->getTotalSegmentsNum();
        }
    }

    if (nParitySegments_ == 0)
    {
        if (meta_)
            nParitySegments_ = meta_->getFrameMeta().parity_size();
        else if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
        {
            std::shared_ptr<const packets::Segment> seg =
                std::dynamic_pointer_cast<const packets::Segment>(dr.getNdnrtcPacket());
            nParitySegments_ = seg->getTotalSegmentsNum();
        }
    }

    size_t dataSize = dr.getData()->getContent().size();
    fetchedBytesTotal_ += dataSize;
    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Data)
    {
        fetchedBytesData_ += dataSize;
        nDataSegmentsFetched_++;
    }
    if (dr.getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
    {
        fetchedBytesParity_ += dataSize;
        nParitySegmentsFetched_++;
    }

    if (nDataSegments_ > 0)
        fetchProgress_ = double(nDataSegmentsFetched_+nParitySegmentsFetched_+2) / double(nDataSegments_+nParitySegments_+2);

    if (metaIsFetched_ &&
        isReadyForDecoder() && slotState_ == PipelineSlotState::Assembling)
    {
        slotState_ = PipelineSlotState::Ready;
        triggerEvent(PipelineSlotState::Ready, dr);
    }
    else
        if (slotState_ < PipelineSlotState::Assembling)
        {
            slotState_ = PipelineSlotState::Assembling;
            triggerEvent(PipelineSlotState::Assembling, dr);
        }
}

void
BufferSlot::triggerEvent(PipelineSlotState s, const DataRequest& dr)
{
    switch (s) {
        case PipelineSlotState::Unfetchable:
            onUnfetchable_(this, dr);
            break;
        case PipelineSlotState::Assembling:
            onAssembling_(this, dr);
            break;
        case PipelineSlotState::Ready:
            onReady_(this, dr);
            break;
        case PipelineSlotState::Pending:
            onPending_(this, dr);
            break;
        default:
            break;
    }
}

std::string
BufferSlot::dump(bool showLastSegment) const
{
    std::stringstream dump;

    dump << "[ "
    // << (nameInfo_.class_ == SampleClass::Delta?"D":((nameInfo_.class_ == SampleClass::Key)?"K":"U")) << ", "
    << nameInfo_.getSuffix(NameFilter::Sample).toUri()
    << std::setw(5)
    << " "
    << nameInfo_.sampleNo_
    << (verified_ == Verification::Verified ? "☆" : (verified_ == Verification::Unknown? "?" : "✕")) << ", "
    << (metaIsFetched_ ? "+m " : "-m ")
    << (manifestIsFetched_ ? "+f " : "-f ")
    << (metaIsFetched_ ? (getMetaPacket()->getFrameMeta().type() == FrameMeta_FrameType_Key ? "K " : "D ") : "? ")
    << (metaIsFetched_ ? getMetaPacket()->getFrameMeta().gop_number() : 0) << "-"
    << (metaIsFetched_ ? getMetaPacket()->getFrameMeta().gop_position() : 0) << " "
    << getFetchedDataSegmentsNum() << "/" << getDataSegmentsNum() << " "
    << getFetchedParitySegmentsNum() << "/" << getParitySegmentsNum() << " "
    << getFetchedBytesData() << "b " << getFetchedBytesParity() << "b "
    //    << toString(getConsistencyState()) << ", "
    // << std::setw(7) << getPlaybackNumber() << ", "
    // << std::setw(10) << getProducerTimestamp() << ", "
    << std::setw(3) << fetchProgress_*100 << "% "//"("
    // << ((double)nSegmentsParityReady_/(double)nSegmentsParity_)*100 << "%), "
    // << std::setw(5) << getPairedFrameNumber() << ", "
    // << std::setw(3) << getPlaybackDeadline() << ", "
    //    << std::setw(3) << nRtx_ << " "
    // << (isRecovered_ ? "R" : "I") << ", "
    // << std::setw(2) << nSegmentsTotal_ << "/" << nSegmentsReady_
    // << "/" << nSegmentsPending_ << "/" << nSegmentsMissing_
    // << "/" << nSegmentsParity_ << " "
    // << getLifetime() << " "
    //    << std::setw(5) << assembledSize_ << " "
    //    << (showLastSegment ? lastFetched_->getInfo().getSuffix(suffix_filter::Thread) : nameInfo_.getSuffix(suffix_filter::Thread))
    //    << " " << (lastFetched_ && lastFetched_->isOriginal() ? "ORIG" : "CACH")
    //    << " dgen " << (showLastSegment ? lastFetched_->getDgen() : -1)
    //    << " rtt " << (showLastSegment ? lastFetched_->getRoundTripDelayUsec()/1000 : -1)
    //    << " " << std::setw(5) << (getConsistencyState() & BufferSlot::HeaderMeta ? getHeader().publishTimestampMs_ : 0)
    << (isReadyForDecoder() ? "+dec " : "-dec ")
    << getFirstRequestTsUsec() << " "
    << getLastDataTsUsec()
    << "]";

    return dump.str();
}


// ----------------------------------------------------------------------------------------
// CODE BELOW IS DEPRECATED
void
BufferSlot::segmentsRequested(const std::vector<std::shared_ptr<const ndn::Interest>>& interests)
{
    if (state_ == Ready || state_ == Locked)
        throw std::runtime_error("Can't add more segments because slot is ready or locked");

    for (auto i:interests)
    {
        std::shared_ptr<SlotSegment> segment(std::make_shared<SlotSegment>(i));

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

const std::shared_ptr<SlotSegment>
BufferSlot::segmentReceived(const std::shared_ptr<WireSegment>& segment)
{
    assert(segment->isValid());

    if (state_ == Locked)
        return std::shared_ptr<SlotSegment>();

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

const std::vector<std::shared_ptr<const ndn::Interest>>
BufferSlot::getPendingInterests() const
{
    std::vector<std::shared_ptr<const ndn::Interest>> pendingInterests;

    for (auto it:requested_)
        if (fetched_.find(it.first) == fetched_.end())
            pendingInterests.push_back(it.second->getInterest());

    return pendingInterests;
}

const std::vector<std::shared_ptr<const SlotSegment>>
BufferSlot::getFetchedSegments() const
{
    std::vector<std::shared_ptr<const SlotSegment>> segments;

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

const CommonHeader
BufferSlot::getHeader() const
{
    if (!consistency_&HeaderMeta)
        throw std::runtime_error("Packet header is not available");

    return fetched_.at(nameInfo_.getSuffix(suffix_filter::Segment))->getData()->packetHeader();
}

void
BufferSlot::updateConsistencyState(const std::shared_ptr<SlotSegment>& segment)
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
            throw std::runtime_error("Trying to lock slot which is not ready");
        state_ = BufferSlot::State::Locked;
    }
}

//******************************************************************************
VideoFrameSlot::VideoFrameSlot(const size_t storageSize):
storage_(std::make_shared<std::vector<uint8_t>>())
{
    storage_->reserve(storageSize);
    fecList_.reserve(1000);
}

std::shared_ptr<ImmutableVideoFramePacket>
VideoFrameSlot::readPacket(const BufferSlot& slot, bool& recovered)
{
    if (slot.getNameInfo().streamType_ !=
        MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    // check if recovery is possible
    Name parityKey(NameComponents::NameComponentParity);
    std::map<ndn::Name, std::shared_ptr<SlotSegment>>
        dataSegments(slot.fetched_.begin(), slot.fetched_.lower_bound(parityKey));
    std::map<ndn::Name, std::shared_ptr<SlotSegment>>
        paritySegments(slot.fetched_.upper_bound(parityKey), slot.fetched_.end());

    if ((!paritySegments.size() &&
        dataSegments.size() < dataSegments.begin()->second->getData()->getSlicesNum()) ||
        dataSegments.size() == 0)
    {
        recovered = false;
        return std::shared_ptr<ImmutableVideoFramePacket>();
    }

    std::shared_ptr<WireData<VideoFrameSegmentHeader>> firstSeg =
            std::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(dataSegments.begin()->second->getData());
    std::shared_ptr<WireData<VideoFrameSegmentHeader>> firstParitySeg;

    if (paritySegments.size())
        firstParitySeg = std::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(paritySegments.begin()->second->getData());

    size_t segmentSize = firstSeg->segment().getPayload().size();
    size_t paritySegSize = (paritySegments.size() ? firstParitySeg->segment().getPayload().size() : 0);
    unsigned int nDataSegmentsExpected = firstSeg->getSlicesNum();
    unsigned int nParitySegmentsExpected = (paritySegments.size() ? paritySegments.begin()->second->getData()->getSlicesNum() : 0);

    fecList_.assign(nDataSegmentsExpected+nParitySegmentsExpected, FEC_RLIST_SYMEMPTY);
    storage_->resize(segmentSize*(nDataSegmentsExpected+nParitySegmentsExpected));

    int segNo = 0;
    for (auto it:dataSegments)
    {
        const std::shared_ptr<WireData<VideoFrameSegmentHeader>> wd =
            std::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(it.second->getData());

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
            const std::shared_ptr<WireData<VideoFrameSegmentHeader>> wd =
            std::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(it.second->getData());

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

    return (frameExtracted ? std::make_shared<ImmutableVideoFramePacket>(storage_) :
                std::shared_ptr<ImmutableVideoFramePacket>());
}

VideoFrameSegmentHeader
VideoFrameSlot::readSegmentHeader(const BufferSlot& slot)
{
    if (slot.getNameInfo().streamType_ !=
        MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    std::map<ndn::Name, std::shared_ptr<SlotSegment>>
        dataSegments(slot.fetched_.begin(), slot.fetched_.lower_bound(Name(NameComponents::NameComponentParity)));

    if (!dataSegments.size())
        return VideoFrameSegmentHeader();

    std::shared_ptr<WireData<VideoFrameSegmentHeader>> seg =
            std::dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(dataSegments.begin()->second->getData());

    return seg->segment().getHeader();
}

//******************************************************************************
AudioBundleSlot::AudioBundleSlot(const size_t storageSize):
storage_(std::make_shared<std::vector<uint8_t>>())
{
    storage_->reserve(storageSize);
}

std::shared_ptr<ImmutableAudioBundlePacket>
AudioBundleSlot::readBundle(const BufferSlot& slot)
{
     if (slot.getNameInfo().streamType_ !=
        MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
        throw std::runtime_error("Wrong slot supplied: can not read video "
            "packet from audio slot");

    if (slot.getAssembledLevel() < 1.)
        return std::shared_ptr<ImmutableAudioBundlePacket>();

    std::shared_ptr<WireData<DataSegmentHeader>> firstSeg =
            std::dynamic_pointer_cast<WireData<DataSegmentHeader>>(slot.fetched_.begin()->second->getData());
    size_t segmentSize = firstSeg->segment().getPayload().size();
    unsigned int nDataSegmentsExpected = firstSeg->getSlicesNum();

    storage_->resize(segmentSize*nDataSegmentsExpected);

    for (auto it:slot.fetched_)
    {
        const std::shared_ptr<WireData<DataSegmentHeader>> wd =
            std::dynamic_pointer_cast<WireData<DataSegmentHeader>>(it.second->getData());
        storage_->insert(storage_->begin(),
                wd->segment().getPayload().begin(),
                wd->segment().getPayload().end());
    }

    return std::make_shared<ImmutableAudioBundlePacket>(storage_);
}

//******************************************************************************
SlotPool::SlotPool(const size_t& capacity):
capacity_(capacity)
{
    for (int i = 0; i < capacity; ++i)
        pool_.push_back(std::make_shared<BufferSlot>());
}

std::shared_ptr<BufferSlot>
SlotPool::pop()
{
    if (pool_.size())
    {
        std::shared_ptr<BufferSlot> slot = pool_.back();
        pool_.pop_back();
        return slot;
    }
    return std::shared_ptr<BufferSlot>();
}

bool
SlotPool::push(const std::shared_ptr<BufferSlot>& slot)
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
Buffer::Buffer(std::shared_ptr<RequestQueue> interestQ,
               shared_ptr<DecodeQueue> decodeQ,
               uint64_t slotRetainIntervalUsec,
               std::shared_ptr<StatisticsStorage> storage)
    : sstorage_(storage)
    , requestQ_(interestQ)
    , decodeQ_(decodeQ)
    , delayEstimateGamma_(4.)
    , delayEstimateTheta_(15./16.)
    , dqFilter_(delayEstimateTheta_)
    , isJitterCompensationOn_(true)
    , slotRetainIntervalUsec_(slotRetainIntervalUsec)
    , cleanupIntervalUsec_(5E6)
    , lastCleanupUsec_(0)
    , slotPushTimer_(interestQ->getIoService())
    , slotPushFireTime_(0)
    , slotPushDelay_(0)
    , lastPushedSampleNo_(0)
{
    assert(sstorage_.get());
    description_ = "buffer";
}

void
Buffer::newSlot(std::shared_ptr<IPipelineSlot> slot)
{
    std::shared_ptr<BufferSlot> s = dynamic_pointer_cast<BufferSlot>(slot);
    if (s)
    {
        SlotEntry e;
        e.slot_ = s;
        e.pushedForDecode_ = false;

        std::shared_ptr<Buffer> me = dynamic_pointer_cast<Buffer>(shared_from_this());
        // request missing segments, if needed
        e.onMissingDataConn_ =
            s->addOnNeedData([this, me, s](IPipelineSlot *sl,
                                        std::vector<std::shared_ptr<DataRequest>> missingR)
                         {
//                             cout << "REQUEST MISSING" << endl;

                             LogTraceC << "request missing " << missingR.size() << " "
                                       << s->dump() << endl;
                             (*sstorage_)[Indicator::DoubleRttNum]++;

                             sl->setRequests(missingR);
                             requestQ_->enqueueRequests(missingR, std::make_shared<DeadlinePriority>(REQ_DL_PRI_RTX));
                         });
        e.onReadyConn_ =
            s->subscribe(PipelineSlotState::Ready,
                     [this, me, s](const IPipelineSlot *sl, const DataRequest& r){
                         LogDebugC << "slot ready " << s->dump() << endl;
                         (*sstorage_)[Indicator::SlotsReadyNum]++;

                         calculateDelay(s->getAssemblingTime());
                         onSlotReady(s);

//                         cout << "SLOT READY. CHECK "
//                              << r.getReplyTimestampUsec() << " " << s->dump() << endl;

                         checkReadySlots(r.getReplyTimestampUsec());
                     });
        e.onUnfetchableConn_ =
            s->subscribe(PipelineSlotState::Unfetchable,
                     [this, me, s](const IPipelineSlot *sl, const DataRequest& r){
                         LogDebugC << "slot unfetchable (reason "
                                   << r.getStatus() << ") " << s->dump() << endl;
                         (*sstorage_)[Indicator::SlotsUnfetchableNum]++;

                         onSlotUnfetchable(s);
                     });
        e.onAssemblingConn_ =
            s->subscribe(PipelineSlotState::Assembling,
                         [this, me, s](const IPipelineSlot *sl, const DataRequest& r){
                             setSlotPushDeadline(s, r.getReplyTimestampUsec());
                         });

        {
            std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
            e.insertedUsec_ = clock::microsecondTimestamp();
            e.pushDeadlineUsec_ = 0;
            slots_[e.slot_->getNameInfo().sampleNo_] = e;

            (*sstorage_)[Indicator::BufferedSlotsNum]++;
        }
    }
    else
        LogErrorC << "couldn't add slot: the slot is not a BufferSlot instance" << endl;
}

void
Buffer::removeSlot(const PacketNumber& no)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    map<PacketNumber, SlotEntry>::iterator it = slots_.find(no);

    if (it != slots_.end())
    {
        it->second.onUnfetchableConn_.disconnect();
        it->second.onAssemblingConn_.disconnect();
        it->second.onReadyConn_.disconnect();
        it->second.onMissingDataConn_.disconnect();
        slots_.erase(it);
    }
    else
    {
        LogDebugC << "trying to remove non-existent slot " << no << endl;
    }
}

void
Buffer::reset()
{
    slots_.clear();
//    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
//
//    for (auto s:activeSlots_)
//        pool_->push(s.second);
//    activeSlots_.clear();
//
//    LogDebugC << "slot pool capacity " << pool_->capacity()
//    << " pool size " << pool_->size() << " "
//    << reservedSlots_.size() << " slot(s) locked for playback" << std::endl;
//
//    for (auto o:observers_) o->onReset();
}

void
Buffer::calculateDelay(double dQ)
{
    dqFilter_.newValue(dQ);
    delayEstimate_ = dqFilter_.value() + delayEstimateGamma_ * requestQ_->getJitterEstimate();
}

void
Buffer::checkReadySlots(uint64_t now)
{
    // 1. go over ready slots
    // 2. check:
    //      - if frame GOP-decodable:
    //          - frame is KEY
    //              - OR -
    //          - frame #-1 is GOP-decodable
    //      -> then
    //          - if time to push frame:
    //              - frame # < last pushed #
    //                  - OR -
    //              - frame is KEY and past push deadline
    //                  - OR -
    //              - frame is NOT KEY
    //          -> then
    //              push frame to decodeQ
    //          -> else
    //              setup/adjust timer
    //      -> else
    //          do nothing
    // 3. check for cleanup of old frames

    {
        vector<shared_ptr<BufferSlot>> slotsForDecode;
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
        for (auto &e : slots_)
        {
            std::shared_ptr<BufferSlot> slot = e.second.slot_;

            if (!e.second.pushedForDecode_ && slot->getState() == PipelineSlotState::Ready)
            {
                PacketNumber sampleNo = slot->getNameInfo().sampleNo_;
                int32_t gopNo = slot->getMetaPacket()->getFrameMeta().gop_number();
                bool isKey = (slot->getMetaPacket()->getFrameMeta().type() == FrameMeta_FrameType_Key);
                bool prevDecodable = (gopDecodeMap_.find(gopNo) != gopDecodeMap_.end() ? (gopDecodeMap_[gopNo] + 1 == sampleNo) : false);
                int64_t d = (int64_t)e.second.pushDeadlineUsec_ - (int64_t)now;

                LogTraceC << sampleNo << " isKey " << isKey
                    << " previous decodable " << prevDecodable
                    << " last GOP decodable " << (gopDecodeMap_.find(gopNo) != gopDecodeMap_.end() ? gopDecodeMap_[gopNo] : 0)
                    << (isKey ? " deadline in " : "") << (isKey ? d : 0)
                    << endl;

                if (isKey || prevDecodable)
                {
                    // we push for decode if any of the following is true:
                    // 1. it's a Key frame and it's time to push it
                    // 2. it's a Delta frame
                    // 3. it's out-of-order old frame
                    bool pushForDecode =
                        (isKey && d <= 0) ||
                        (!isKey) ||
                        (sampleNo < lastPushedSampleNo_);
                        // (lastPushedSlot_ && sampleNo < lastPushedSlot_->getNameInfo().sampleNo_);

                    // TODO: check for getIsJitterCompensationOn()
                    if (pushForDecode)
                    {
                        // this is the adjustment for d (since we will never be accurate on push deadline)
                        // slotPusDelay_ should be accounted for in the next push deadline calculation
                        if (isKey)
                            slotPushDelay_ += d;

                        // push to decodeQ
                        LogDebugC << "decodeQ push now. late by "
                                  << d << "usec "
                                  << slot->dump() << endl;

                        e.second.pushedForDecode_ = true;
                        lastPushedSampleNo_ = sampleNo;
                        gopDecodeMap_[gopNo] = sampleNo;

                        slotsForDecode.push_back(slot);
                    }
                    else
                    {
                        // setup timer to push this frame at a later time
                        LogDebugC << "decodeQ push later. in "
                                  << d << "usec "
                                  << slot->dump() << endl;
                    }
                }
            }
        }

        // push slots for decode and remove them from the buffer
        for (auto &s: slotsForDecode)
        {
            removeSlot(s->getNameInfo().sampleNo_);
            decodeQ_->push(s);
        }
    }

    doCleanup(now);
}

void
Buffer::doCleanup(uint64_t now)
{
    if (lastCleanupUsec_ == 0 || now - lastCleanupUsec_ >= cleanupIntervalUsec_)
    {
        // cout << "CLEANUP " << dump() << endl;

        // iterate over slots and remove all that have insertedUsec timestamp older
        // than slotRetainInterval
        vector<PacketNumber> oldSlots;
        for (auto &e : slots_)
        {
            if (now > e.second.insertedUsec_ &&
                (now - e.second.insertedUsec_ >= slotRetainIntervalUsec_ ||
                e.second.pushedForDecode_))
            {
                // cout << "SLOT DISCARD "
                //      << now << " "
                //      << e.second.insertedUsec_ << " "
                //      << e.second.slot_->getFirstRequestTsUsec() << " "
                //      << slotRetainIntervalUsec_ << " "
                //      << now - e.second.insertedUsec_ << " "
                //      << e.second.slot_->dump() << endl;

                LogTraceC << "old slot discard " << e.second.slot_ << endl;
                oldSlots.push_back(e.second.slot_->getNameInfo().sampleNo_);
            }
        }

        for (auto &s : oldSlots)
        {
            std::shared_ptr<BufferSlot> discardedSlot = slots_[s].slot_;
            removeSlot(s);
            onSlotDiscard(discardedSlot);
        }

        lastCleanupUsec_ = now;
    }
}

void
Buffer::setSlotPushDeadline(const std::shared_ptr<BufferSlot> &s, uint64_t now)
{
    uint64_t delayUsec = (uint64_t)getDelayEstimate();
    uint64_t delayAdjustedUsec = delayUsec;
    uint64_t pushDeadlineUsec = now;

    // account for dealyed pushes from previous iterations
    if (delayUsec + slotPushDelay_ < 0)
    {
        slotPushDelay_ += delayUsec;
        delayAdjustedUsec = 0;
    }
    else
    {
        delayAdjustedUsec += slotPushDelay_;
        slotPushDelay_ = 0;
    }

    pushDeadlineUsec += delayAdjustedUsec;

    slots_[s->getNameInfo().sampleNo_].pushDeadlineUsec_ = pushDeadlineUsec;

    setupPushTimer(pushDeadlineUsec); // ???

    LogTraceC << "decodeQ push deadline is "
              << delayAdjustedUsec << "usec from now. (non-adjusted "
              << delayUsec << "usec)"
              << s->dump() << endl;
}

void
Buffer::setupPushTimer(uint64_t deadline)
{

}

std::string
Buffer::dump() const
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    int i = 0;
    stringstream ss;
    ss << "buffer dump:";

    for (auto& s:slots_)
        ss << std::endl << ++i << " " << s.second.slot_->dump();

    return ss.str();
}

std::string
Buffer::shortdump() const
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    stringstream ss;
    ss << "[ ";

    //dumpSlotDictionary(ss, reservedSlots_);
    dumpSlotDictionary(ss, activeSlots_);

    ss << " ]";

    return ss.str();
}


// CODE BELOW IS DEPRECATED
//Buffer::Buffer(std::shared_ptr<StatisticsStorage> storage,
//               std::shared_ptr<SlotPool> pool):pool_(pool),
//sstorage_(storage)
//{
//    assert(sstorage_.get());
//    description_ = "buffer";
//}

bool
Buffer::requested(const std::vector<std::shared_ptr<const ndn::Interest>>& interests)
{
    std::map<Name, std::vector<std::shared_ptr<const Interest>>> slotInterests;
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
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
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
Buffer::received(const std::shared_ptr<WireSegment>& segment)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);

    BufferReceipt receipt;
    Name key = segment->getInfo().getPrefix(prefix_filter::Sample);

    if (activeSlots_.find(key) == activeSlots_.end())
    {
        stringstream ss;
        ss << "Received data that was not previously requested: "
        << key;
        throw std::runtime_error(ss.str());
    }

    BufferSlot::State oldState = BufferSlot::Free; // activeSlots_[key]->getState();
    receipt.segment_ = activeSlots_[key]->segmentReceived(segment);
    receipt.slot_ = activeSlots_[key];
    receipt.oldState_ = oldState;

//    if (receipt.slot_->getState() == BufferSlot::Ready)
    if (true)
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
Buffer::isRequested(const std::shared_ptr<WireSegment>& segment) const
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    Name key = segment->getInfo().getPrefix(prefix_filter::Sample);

    return (activeSlots_.find(key) != activeSlots_.end());
}

unsigned int
Buffer::getSlotsNum(const ndn::Name& prefix, int stateMask) const
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    unsigned int nSlots = 0;

    for (auto it:activeSlots_)
//        if (prefix.match(it.first) && it.second->getState()&stateMask)
            nSlots++;

    return nSlots;
}

void
Buffer::attach(IBufferObserver* observer)
{
    if (observer)
    {
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
        observers_.push_back(observer);
    }
}

void
Buffer::detach(IBufferObserver* observer)
{
    std::vector<IBufferObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end())
    {
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
        observers_.erase(it);
    }
}

void
Buffer::invalidate(const Name& slotPrefix)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    assert(activeSlots_.find(slotPrefix) != activeSlots_.end());

    std::shared_ptr<BufferSlot> slot = activeSlots_.find(slotPrefix)->second;
    activeSlots_.erase(activeSlots_.find(slotPrefix));

    (*sstorage_)[Indicator::DroppedNum]++;
//    if (slot->getState() <= BufferSlot::Assembling)
        (*sstorage_)[Indicator::IncompleteNum]++;
    if (slot->getNameInfo().class_ == SampleClass::Key)
    {
        (*sstorage_)[Indicator::DroppedKeyNum]++;
//        if (slot->getState() <= BufferSlot::Assembling)
            (*sstorage_)[Indicator::IncompleteKeyNum]++;
    }

    pool_->push(slot);
}

void
Buffer::invalidatePrevious(const Name& slotPrefix)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, std::shared_ptr<BufferSlot>>::iterator lower =
        activeSlots_.lower_bound(slotPrefix);

    while (activeSlots_.begin() != lower)
    {
        LogDebugC << "invalidate " << activeSlots_.begin()->first << std::endl;
        std::shared_ptr<BufferSlot> slot = activeSlots_.begin()->second;

        (*sstorage_)[Indicator::DroppedNum]++;
//        if (slot->getState() <= BufferSlot::Assembling)
            (*sstorage_)[Indicator::IncompleteNum]++;
        if (slot->getNameInfo().class_ == SampleClass::Key)
        {
            (*sstorage_)[Indicator::DroppedKeyNum]++;
//            if (slot->getState() <= BufferSlot::Assembling)
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
Buffer::reserveSlot(const std::shared_ptr<const BufferSlot>& slot)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, std::shared_ptr<BufferSlot>>::iterator it =
    activeSlots_.find(slot->getPrefix());

    if (it != activeSlots_.end())
    {
        reservedSlots_[slot->getPrefix()] = activeSlots_[slot->getPrefix()];
        activeSlots_.erase(it);
        reservedSlots_[slot->getPrefix()]->toggleLock();
    }
}

void
Buffer::releaseSlot(const std::shared_ptr<const BufferSlot>& slot)
{
    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
    std::map<ndn::Name, std::shared_ptr<BufferSlot>>::iterator it =
    reservedSlots_.find(slot->getPrefix());

    if (it != reservedSlots_.end())
    {
        pool_->push(it->second);
        reservedSlots_.erase(it);
    }
}

void
Buffer::dumpSlotDictionary(stringstream& ss,
    const map<ndn::Name, std::shared_ptr<BufferSlot>> &slotDict) const
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

shared_ptr<const VideoCodec::Image> DecodeQueue::ZeroImage = make_shared<const VideoCodec::Image>(0,0,ImageFormat::I420);
//******************************************************************************

const EncodedFrame&
DecodeQueue::BitstreamData::loadFrom(shared_ptr<const BufferSlot>& slot,
    bool& recovered)
{
    assert(slot);
    assert(slot->isReadyForDecoder());

    if (slot == slot_)
        return encodedFrame_;

    // 1. copy all (data and parity) segments from the slot to the bitstream storage
    // 2. check whether frame must (and can) be recovered
    // 3. recover frame if needed
    // 4. wrap EncodedFrame structure around resulting bitstream data

    clear();
    slot_ = slot;
    size_t segmentSize = 0; // we assume const segment sizes
    fecList_.assign(slot->getDataSegmentsNum()+slot->getParitySegmentsNum(), FEC_RLIST_SYMEMPTY);

    for (auto &r : slot->getRequests())
    {
        SegmentClass sc = r->getNamespaceInfo().segmentClass_;

        if (r->getStatus() == DataRequest::Status::Data &&
            (sc == SegmentClass::Parity || sc == SegmentClass::Data))
        {
            uint32_t segNo = r->getNamespaceInfo().segNo_;
            auto d = r->getData();

            if (sc == SegmentClass::Parity)
                segNo += slot->getDataSegmentsNum();
            if (!segmentSize)
            {
                segmentSize = d->getContent().size();
                bitstreamData_.reserve(segmentSize*(slot->getDataSegmentsNum() + slot->getParitySegmentsNum()));
                bitstreamData_.resize(segmentSize*slot->getDataSegmentsNum());
            }

            copy(d->getContent()->begin(), d->getContent()->end(),
                 bitstreamData_.begin()+segmentSize*segNo);
            fecList_[segNo] = FEC_RLIST_SYMREADY;
        }
    }

    // check if need to recover using FEC data
    if (slot->getFetchedDataSegmentsNum() < slot->getDataSegmentsNum())
    {
        fec::Rs28Decoder dec(slot->getDataSegmentsNum(), slot->getParitySegmentsNum(),
                             segmentSize);
        int nRecovered = dec.decode(bitstreamData_.data(),
                                    bitstreamData_.data() + segmentSize*slot->getDataSegmentsNum(),
                                    fecList_.data());
        recovered = (nRecovered + slot->getFetchedDataSegmentsNum() >= slot->getDataSegmentsNum());
    }
    else
        recovered = false;

    encodedFrame_.type_ = slot->getMetaPacket()->getFrameMeta().type() == FrameMeta_FrameType_Key ? FrameType::Key : FrameType::Delta;
    encodedFrame_.data_ = bitstreamData_.data();
    encodedFrame_.length_ = bitstreamData_.size();

    return encodedFrame_;
}

void
DecodeQueue::VpxExternalBufferList::DecodedData::clear()
{
    inUseByDecoder_ = false;
    img_ = std::const_pointer_cast<VideoCodec::Image>(ZeroImage);
//    rawData_.clear();
}

void
DecodeQueue::VpxExternalBufferList::DecodedData::memset(uint8_t c)
{
    ::memset(rawData_.data(), c, rawData_.capacity());
}

void
DecodeQueue::VpxExternalBufferList::DecodedData::setImage(const VideoCodec::Image& img,
    const NamespaceInfo& ni)
{
    ni_ = ni;

    assert(img.getDataSize());
    assert(img.getIsWrapped());

    img_ = make_shared<VideoCodec::Image>(img.getVpxImage());
    img_->setUserData((void*)&ni_);
}

const VideoCodec::Image&
DecodeQueue::VpxExternalBufferList::DecodedData::getImage() const
{
    return *img_;
}

DecodeQueue::VpxExternalBufferList::VpxExternalBufferList(uint32_t capacity)
: framePool_(capacity)
{}

void
DecodeQueue::VpxExternalBufferList::recycle(DecodedData *dd)
{
    assert(dd);

    shared_ptr<DecodedData> ddPtr = getPointer(dd);
    assert(ddPtr);

    if (ddPtr->inUseByDecoder_)
    {
        LogTraceC << "in use by decoder or locked. purge later" << endl;
        delayedPurgeList_.push_back(ddPtr);
    }
    else
        framePool_.push(ddPtr);

    remove(dataInUse_.begin(), dataInUse_.end(), ddPtr);
    doCleanup();
}

shared_ptr<DecodeQueue::VpxExternalBufferList::DecodedData>
DecodeQueue::VpxExternalBufferList::getPointer(DecodedData *dd)
{
    auto it = find_if(dataInUse_.begin(), dataInUse_.end(),
                [&](shared_ptr<DecodedData>& ddPtr){
                     return ddPtr.get() == dd;
                 });

    if (it != dataInUse_.end())
        return *it;

    return shared_ptr<DecodedData>();
}

void
DecodeQueue::VpxExternalBufferList::doCleanup()
{
    auto it = delayedPurgeList_.begin();

    while (it != delayedPurgeList_.end())
    {
        if ((*it)->inUseByDecoder_)
        {
            framePool_.push(*it);
            it = delayedPurgeList_.erase(it);
        }
    }
}

int
DecodeQueue::VpxExternalBufferList::getFreeFrameBuffer(size_t minSize,
                                                       vpx_codec_frame_buffer_t *fb)
{
    auto dd = framePool_.pop();
    if (!dd)
    {
        LogErrorC << "FATAL: no free memory for decoded data. "
                  << framePool_.size() << " decoded frames in-memory"
                  << endl;
        return 1;
    }
    else
    {
        if (dd->getCapacity() < minSize)
        {
            dd->reserve(minSize);
            dd->memset(0);
        }

        dd->resize(minSize);

        fb->data = dd->getData();
        fb->size = dd->getSize();
        fb->priv = dd.get();
        dd->inUseByDecoder_ = true;
        dataInUse_.push_back(dd);
    }

    return 0;
}

int
DecodeQueue::VpxExternalBufferList::returnFrameBuffer(vpx_codec_frame_buffer_t *fb)
{
    auto *dd = (DecodedData*)fb->priv;
    assert(dd);
    dd->inUseByDecoder_ = false;

    return 0;
}

size_t
DecodeQueue::VpxExternalBufferList::getUsedBufferNum()
{
    return framePool_.capacity() - framePool_.size();
}

size_t
DecodeQueue::VpxExternalBufferList::getFreeBufferNum()
{
    return framePool_.size();
}

DecodeQueue::DecodeQueue(boost::asio::io_service& io, uint32_t size, uint32_t capacity)
: io_(io)
, size_(size)
, bitstreamDataPool_(capacity)
, vpxFbList_(capacity)
, pbPointer_(frames_.end())
, lastAsked_(-1)
, lastRetrieved_(frames_.end())
, lockedRecycleData_(nullptr)
, discardGop_(-1)
, isCheckFramesScheduled_(false)
{
    setDescription("decodeQ");
    CodecSettings codecSettings;// = VideoCodec::defaultCodecSettings();
    codecSettings.numCores_ = thread::hardware_concurrency();
    codecSettings.rowMt_ = true;
    codecSettings.spec_.decoder_.frameBufferList_ = &vpxFbList_;
    codecContext_.codec_.initDecoder(codecSettings);
    resetCodecContext();
}

void
DecodeQueue::push(const shared_ptr<const BufferSlot>& slot)
{
    lock_guard<recursive_mutex> mLock(masterMtx_);
    pair<FrameQueue::iterator, bool> res = frames_.insert(FrameQueueEntry{slot,
                                bitstreamDataPool_.pop(),
                                nullptr});
    // result will be false if frame with same number is already in the queue
    assert(res.second);

    LogTraceC << slot->dump() << endl;

    int32_t gopNo = slot->getMetaPacket()->getFrameMeta().gop_number();
    bool isKey = (slot->getMetaPacket()->getFrameMeta().type() == FrameMeta_FrameType_Key);
    PacketNumber sampleNo = slot->getNameInfo().sampleNo_;

    if (gops_.find(gopNo) == gops_.end())
    {
        gops_[gopNo] = {gopNo, 0, frames_.end(), frames_.end()};
        LogTraceC << "new gop " << gopNo << endl;
    }

    if (isKey)
    {
        gops_[gopNo].start_ = res.first;
        LogTraceC << "gop " << gopNo << " start " << sampleNo << endl;
    }

    // check if we can close gop boundary
    //   - current frame is GOP start -> check previous GOP
    //   - current frame is the last one in its' GOP --> check next GOP
    {
        int32_t prevGop = (gops_.find(gopNo-1) == gops_.end() ? -1 : gopNo-1);
        int32_t nextGop = (gops_.find(gopNo+1) == gops_.end() ? -1 : gopNo+1);

        if (prevGop >= 0 && res.first != frames_.begin())
        {
            shared_ptr<const BufferSlot> prevSlot = prev(res.first)->slot_;
            if (prevSlot->getMetaPacket()->getFrameMeta().gop_number() == prevGop)
            {
                gops_[prevGop].end_ = prev(res.first);

                LogTraceC << "gop " << prevGop << " end " << prevSlot->getNameInfo().sampleNo_ << endl;
            }
        }

        if (nextGop > 0 && next(res.first) != frames_.end())
        {
            shared_ptr<const BufferSlot> nextSlot = next(res.first)->slot_;
            if (nextSlot->getMetaPacket()->getFrameMeta().gop_number() == nextGop)
            {
                gops_[gopNo].end_ = res.first;

                LogTraceC << "gop " << gopNo << " end " << sampleNo << endl;
            }
        }
    }

    if (pbPointer_ == frames_.end())
    {
        lock_guard<recursive_mutex> lock(playbackMtx_);
        
        pbPointer_ = res.first;
        LogTraceC << "set pb pointer " << pbPointer_->slot_->dump() << endl;
    }

    if (!isCheckFramesScheduled_)
    {
        isCheckFramesScheduled_ = true;
        shared_ptr<DecodeQueue> me = dynamic_pointer_cast<DecodeQueue>(shared_from_this());
        async::dispatchAsync(io_, bind(&DecodeQueue::checkFrames, me));
    }
}

const VideoCodec::Image&
DecodeQueue::get(int32_t step, bool allowDecoding)
{
    VpxExternalBufferList::DecodedData* rawData = nullptr;
    
    {
        lock_guard<recursive_mutex> pLock(playbackMtx_);
        
        if (pbPointer_ != frames_.end())
        {
            LogTraceC << pbPointer_->slot_->dump() << endl;
            
            if (pbPointer_->rawData_)
                rawData = pbPointer_->rawData_;
            else if (allowDecoding)
            {
                LogTraceC << "requested frame not decoded. trying to decode" << endl;
                
                // try to decode now
                int32_t gopNo = pbPointer_->slot_->getMetaPacket()->getFrameMeta().gop_number();
                assert(gops_.find(gopNo) != gops_.end());
                
                if (gops_[gopNo].start_ != frames_.end())
                {
                    lock_guard<recursive_mutex> cLock(codecContext_.mtx_);
                    
                    if (codecContext_.lastDecoded_ != frames_.end() &&
                        frameIteratorWithinRange(next(codecContext_.lastDecoded_),
                                                 gops_[gopNo].start_, pbPointer_))
                        runDecoder(codecContext_, next(codecContext_.lastDecoded_),
                                   distance(codecContext_.lastDecoded_, pbPointer_));
                    else
                    {
                        runDecoder(codecContext_, gops_[gopNo].start_, distance(gops_[gopNo].start_, pbPointer_)+1);
                        resetCodecContext();
                    }
                    
                    if (!pbPointer_->rawData_)
                        return *ZeroImage;
                    
                    assert(pbPointer_->rawData_->hasImage());
                    rawData = pbPointer_->rawData_;
                }
                else
                {
                    LogErrorC << "frame is not decodable" << endl;
                    assert(false);
                }
            }
            else
                return *ZeroImage;
            
            // {
            // lock_guard<recursive_mutex> lock(pbMutex_);
            lastRetrieved_ = pbPointer_;
            // }
            //        seek(step);
            
            //        return rawData->getImage();
        }
    }
    
    if (step) seek(step);

    return rawData ? rawData->getImage() : *ZeroImage;
}

int32_t
DecodeQueue::seek(int32_t step)
{
    if (!step) return 0;

    lock_guard<recursive_mutex> lock(masterMtx_);
    int32_t maxAdvance = 0;

    if (step < 0)
        maxAdvance = -distance(frames_.begin(), pbPointer_);
    else
    {
        if (pbPointer_ == frames_.end())
            maxAdvance = 0;
        else
            maxAdvance = distance(pbPointer_, prev(frames_.end()));
    }

    int32_t d = 0;

    if (abs(step) >= abs(maxAdvance))
    {
        d = maxAdvance;
        advance(pbPointer_, maxAdvance);
    }
    else
    {
        d = step;
        advance(pbPointer_, step);
    }

    if (d != 0 && lockedRecycleData_)
    {
        vpxFbList_.recycle(lockedRecycleData_);
        lockedRecycleData_ = nullptr;
    }

    LogTraceC << "advance pb pointer by " << d << ": "
        << (pbPointer_ == frames_.end() ? "" : pbPointer_->slot_->dump())
        << endl;
    return d;
}

void
DecodeQueue::checkFrames()
{
    // checks what frame can be and should be decoded next
    // it follows this logic in choosing next frame/gop to decode:
    // - continue decoding GOP_cur (current gop)
    // - if no current gop OR current gop decoded:
    //      - find encoded frame E that is the closest to the pbPointer (or lastRetrieved)
    //      - set GOP_cur = GOP_e (E frames' GOP)
    // - if no closest encoded frame E:
    //      - set GOP_cur = GOP_cur + 1
    // - if no GOP_cur + 1:
    //      - GOP_cur = pbPointer gop, if has encoded frames
    // - otherwise:
    //      - scan all GOPs and select first GOP that has any encoded frames
{
    lock_guard<recursive_mutex> mLock(masterMtx_);
    LogTraceC << dump() << endl;

    int32_t lastDecodedSampleNo = -1;
    int32_t curGopCanDecode = 0;

    int32_t curSampleNo = -1;
    int32_t leftToDecode = 0;
    FrameQueue::iterator decodeStart = frames_.end();

    if (codecContext_.lastDecoded_ != frames_.end())
    {
        shared_ptr<const BufferSlot> lastDecodedSlot = codecContext_.lastDecoded_->slot_;
        lastDecodedSampleNo = lastDecodedSlot->getNameInfo().sampleNo_;
        GopEntry ge = gops_[lastDecodedSlot->getMetaPacket()->getFrameMeta().gop_number()];
        leftToDecode = getEncodedNum(ge);

        if (leftToDecode)
            decodeStart = next(codecContext_.lastDecoded_);
    }
    assert(leftToDecode >= 0);

    // find encoded frame that is the closest to the pbPointer
    if (!leftToDecode)
    {
        // lock_guard<recursive_mutex> lock(pbMutex_);
        FrameQueue::iterator p = (pbPointer_ == frames_.end() ? lastRetrieved_ : pbPointer_);

        if (p != frames_.end())
        {
            int32_t closestEncodedLeftOffset = getClosest(p, true, false);
            int32_t closestEncodedRightOffset = getClosest(p, true, true);

            if (closestEncodedRightOffset != abs(closestEncodedLeftOffset))
            {
                int32_t offset = 0;
                if (closestEncodedRightOffset < abs(closestEncodedLeftOffset))
                    offset = closestEncodedRightOffset;
                else
//                if (closestEncodedRightOffset > abs(closestEncodedLeftOffset))
                    offset = closestEncodedLeftOffset;

                if (offset)
                {
                    advance(p, offset);
                    GopEntry ge = gops_[p->slot_->getMetaPacket()->getFrameMeta().gop_number()];

                    decodeStart = ge.start_;
                    advance(decodeStart, ge.nDecoded_);
                    leftToDecode = getEncodedNum(ge);

                    if (leftToDecode)
                    {
                        codecContext_.gopStart_ = ge.start_;
                        codecContext_.gopEnd_ = ge.end_;

                        LogTraceC << "closest to decode " << decodeStart->slot_->dump() << endl;
                    }
                }
            }
        }
    }
    assert(leftToDecode >= 0);
    
    // prioritise pbPointer gop
    if (!leftToDecode)
    {
        if (pbPointer_ != frames_.end())
        {
            GopEntry &ge = gops_[pbPointer_->slot_->getMetaPacket()->getFrameMeta().gop_number()];
            decodeStart = ge.start_;
            advance(decodeStart, ge.nDecoded_);
            leftToDecode = getEncodedNum(ge);
            
            if (leftToDecode)
            {
                codecContext_.gopStart_ = ge.start_;
                codecContext_.gopEnd_ = ge.end_;

                LogTraceC << "decode by pb pointer " << decodeStart->slot_->dump() << endl;
            }
        }
    }
    assert(leftToDecode >= 0);

    // scan all GOPs and find a candidate that has encoded frames
    if (!leftToDecode)
    {
        for (auto &it : gops_)
        {
            GopEntry &ge = it.second;
            // has Key frame to start decoding -- good
            if (ge.start_ != frames_.end())
            {
                // calculate how many frames left to decode (include +1 if GOP end is not frames_.end())
                int32_t gopLeftToDecode = getEncodedNum(ge);

                if (gopLeftToDecode > 0)
                {
                    decodeStart = ge.start_;
                    leftToDecode = gopLeftToDecode;
                    codecContext_.gopStart_ = ge.start_;
                    codecContext_.gopEnd_ = ge.end_;

                    LogTraceC << "gop candidate " << ge.no_
                              << " left to decode " << gopLeftToDecode << endl;

                    break;
                }
            }
        }
    }
    assert(leftToDecode >= 0);

    if (leftToDecode > 0)
    {
        lock_guard<recursive_mutex> cLock(codecContext_.mtx_);
        
        runDecoder(codecContext_, decodeStart, leftToDecode, 30);
        if (distance(decodeStart, codecContext_.lastDecoded_) < leftToDecode)
        {
            // schedule again
            shared_ptr<DecodeQueue> me = dynamic_pointer_cast<DecodeQueue>(shared_from_this());
            async::dispatchAsync(io_, bind(&DecodeQueue::checkFrames, me));
        }
        else
            isCheckFramesScheduled_ = false;
    }
    else
    {
        isCheckFramesScheduled_ = false;
        LogTraceC << "nothing to decode" << endl;
    }
    
#if 0
    //    if (codecContext_.lastDecoded_ != frames_.end())
    //    {
    //        int32_t curGop = -1;
    //        shared_ptr<const BufferSlot> lastDecodedSlot = codecContext_.lastDecoded_->slot_;
    //        lastDecodedSampleNo = lastDecodedSlot->getNameInfo().sampleNo_;
    //        curGop = lastDecodedSlot->getMetaPacket()->getFrameMeta().gop_number();
    //
    //        if (gops_.find(curGop) != gops_.end())
    //            codecContext_.gopEnd_ = gops_[curGop].end_;
    //        curGopCanDecode = distance(codecContext_.lastDecoded_, codecContext_.gopEnd_);
    //
    //        if (codecContext_.gopEnd_ == frames_.end())
    //            curGopCanDecode -= 1;
    //    }

    GopEntry candidateGop {-1, 0, frames_.end(), frames_.end()};
    // find new candidates for decoding
    for (auto &it : gops_)
    {
        GopEntry &ge = it.second;

        // has Key frame to start decoding -- good
        if (ge.start_ != frames_.end())
        {
            // calculate how many frames left to decode (include +1 if GOP end is not frames_.end())
            int32_t leftToDecode = distance(ge.start_, ge.end_) - ge.nDecoded_
                    + (ge.end_ == frames_.end() ? 0 : 1);

            LogTraceC << "left to decode " << ge.no_ << " " << leftToDecode << endl;

            if (leftToDecode > 0)
            {
                candidateGop = ge;
                break;
            }

//            if (ge.end_ != frames_.end())
//            {
//                gopSize = distance(ge.start_, ge.end_)+1;
//                leftToDecode = distance(ge.start_, ge.end_)+1 - ge.nDecoded_;
//            }
//            else
//            {
//                if (ge.nDecoded_ > distance(ge.start_, ge.end_);
//            }
//
//            // if we don't have last frame in this GOP, check if there are frames to decode
//            if (leftToDecode == -1)
//            {
//                FrameQueue::iterator it(ge.start_);
//                advance(it, ge.nDecoded_);
//
//                if (it != frames_.end())
//                {
//                    candidateGop = ge;
//                    break;
//                }
//                // else -- there's nothing to do
//            }
//            else if (leftToDecode > 0)
//            {
//                candidateGop = ge;
//                break;
//            }
        }
    }


    // check if we can continue decoding what's already is being decoded
    if (lastDecodedSampleNo >= 0 && curGopCanDecode)
    {
        LogTraceC << "continue decode " << next(codecContext_.lastDecoded_)->slot_->dump () << endl;

        runDecoder(codecContext_, next(codecContext_.lastDecoded_), curGopCanDecode, 50);
    }
    else if (candidateGop.no_ >= 0)
    {
        LogTraceC << "start decode " << candidateGop.start_->slot_->dump() << endl;

        codecContext_.gopStart_ = candidateGop.start_;
        runDecoder(codecContext_, candidateGop.start_,
                   distance(candidateGop.start_, candidateGop.end_), 50);
    }
#endif
    }

    doCleanup();
}

void
DecodeQueue::runDecoder(CodecContext &ctx, FrameQueue::iterator start,
                        int32_t nToDecode, uint32_t timeLimit)
{
    FrameQueue::iterator& decodeIt = start;

    LogTraceC << nToDecode << " frames. starting from " << decodeIt->slot_->dump() << endl;

    uint64_t decodingTime = 0;
    for (int i = 0; i < nToDecode; ++i)
    {
        if (timeLimit && decodingTime > timeLimit)
        {
            LogTraceC << "time limit reached: " << decodingTime << endl;
            break;
        }

        if (checkAssignMemory(decodeIt))
        {
            bool recovered = false;
            shared_ptr<const BufferSlot> slot = decodeIt->slot_;
            GopEntry& ge = gops_[slot->getMetaPacket()->getFrameMeta().gop_number()];
            EncodedFrame ef = decodeIt->bitstreamData_->loadFrom(slot, recovered);

            // LogTraceC << "poking " << decodeIt->slot_->dump() << endl;

            if (ef.data_)
            {
                // check if this frame has been already decoded
                // if so, recycle decoded data
                // (we have to decode this frame again in order to setup codec context
                // for the final frame)
                if (decodeIt->rawData_)
                {
                    vpxFbList_.recycle(decodeIt->rawData_);
                    decodeIt->rawData_ = nullptr;
                }

                if (vpxFbList_.getFreeBufferNum())
                {
                    LogTraceC << "will decode " << slot->dump()
                              << " free slots " << vpxFbList_.getFreeBufferNum() << endl;

                    uint64_t decodingStart = clock::millisecondTimestamp();
                    int res = ctx.codec_.decode(ef, [&](const VideoCodec::Image& img){
                        LogDebugC << "decoded " << slot->getNameInfo().sampleNo_
                        << " " << img.getWidth() << "x" << img.getHeight()
                        << ". fec used: " << (recovered ? "YES " : "NO ")
                        << slot->dump() << endl;

                        VpxExternalBufferList::DecodedData* ddPtr = (VpxExternalBufferList::DecodedData*)img.getVpxImage()->fb_priv;
                        assert(ddPtr);

                        decodeIt->rawData_ = ddPtr;
                        ddPtr->setImage(img, slot->getNameInfo());

                        ctx.lastDecoded_ = decodeIt;
                        ge.nDecoded_++;
                        decodingTime += (clock::millisecondTimestamp() - decodingStart);
                    });

                    if (res)
                    {
                        LogErrorC << "decode error " << slot->dump() << endl;
                        break;
                    }

                    ctx.lastDecoded_ = decodeIt;
                }
                else
                {
                    LogErrorC << "no free memory for decoding" << endl;
                    break;
                }

                decodeIt = next(decodeIt);
                if (decodeIt == frames_.end())
                {
                    assert(codecContext_.lastDecoded_ != frames_.end());
                    return;
                }
            }
            else
                LogErrorC << "fail acquiring slot data " << slot->dump() << endl;
        }
        else
        {
            LogWarnC << "unable to assign pre-allocated memory. "
                        "increase pool capacity to silence this warning."
                        " pool sizes: "
                     << bitstreamDataPool_.size()
                     << " " << vpxFbList_.getFreeBufferNum() << endl;
            break;
        }
    }
}

void
DecodeQueue::resetCodecContext()
{
    {
        lock_guard<recursive_mutex> lock(codecContext_.mtx_);

        codecContext_.lastDecoded_ =
            codecContext_.gopStart_ =
            codecContext_.gopEnd_ = frames_.end();

        LogTraceC << "reset" << endl;
    }
}

void
DecodeQueue::doCleanup()
{
    lock_guard<recursive_mutex> mLock(masterMtx_);
    lock_guard<recursive_mutex> pLock(playbackMtx_);
    lock_guard<recursive_mutex> cLock(codecContext_.mtx_);

    // discard old slots
    // since decode queue provides random access, we can't discard only based on frame numbers
    // slots can be discarded from either end of the queue
    //   - check lastRetrieved_ GOP
    //          if it's closer to the end -- remove from the front
    //          if it's closer to the front -- remove from the end
    //          if it's undefined -- assume forward access and remove from the front
    //   - update playback pointer as needed

    // slots discarded only from the ends of the queue
    // discard slots policy (checks in descending priority order):
    // - decoded frame that is farthest from the playback pointer (or lastRetrieved)
    // - previously selected GOP
    // - edge frame of one of two edge GOPs which has max nDecoded
    // - frame from the front of the queue
    while (frames_.size() > size_)
    {
        FrameQueue::iterator removeIt = frames_.end();
        FrameQueue::iterator p;

        // {
            // lock_guard<recursive_mutex> lock(pbMutex_);
            p = (pbPointer_ == frames_.end() ? lastRetrieved_ : pbPointer_);
        // }

        // check farthest frames
        if (p != frames_.end())
        {
            int32_t farthestDecodedLeftOffset = getFarthest(p, false, false);
            int32_t farthestDecodedRightOffset = getFarthest(p, false, true);

            if (farthestDecodedRightOffset != abs(farthestDecodedLeftOffset))
            {
                int32_t offset = 0;
                if (farthestDecodedRightOffset > abs(farthestDecodedLeftOffset))
                    offset = farthestDecodedRightOffset;
                else
                    offset = farthestDecodedLeftOffset;

                if (offset)
                {
                    removeIt = p;
                    advance(removeIt, offset);

                    LogTraceC << "discard farthest decoded " << removeIt->slot_->dump() << endl;
                }
            }
        }

        // check GOPs with max nDecoded
        if (removeIt == frames_.end())
        {
            GopEntry leftGop = gops_.begin()->second;
            GopEntry rightGop = gops_.rbegin()->second;

            if (leftGop.nDecoded_ > rightGop.nDecoded_)
            {
//                int32_t offset = distance(frames_.begin(), leftGop.end_);
                removeIt = frames_.begin();
                discardGop_ = leftGop.no_;

                LogTraceC << "discard left GOP edge frame " << removeIt->slot_->dump() << endl;
            }
            else if (rightGop.nDecoded_ > leftGop.nDecoded_)
            {
                removeIt = prev(frames_.rbegin().base());
                discardGop_ = rightGop.no_;

                LogTraceC << "discard right GOP edge frame " << removeIt->slot_->dump() << endl;
            }
        }

        // previously selected gop
        if (removeIt == frames_.end() && discardGop_ != -1)
        {
            if (gops_.begin()->first == discardGop_)
            {
                removeIt = frames_.begin();

                LogTraceC << "discard edge frame from " << discardGop_
                          << " " << removeIt->slot_->dump() << endl;
            }
            else if (gops_.rbegin()->first == discardGop_)
            {
                removeIt = prev(frames_.rbegin().base());

                LogTraceC << "discard edge frame from " << discardGop_
                << " " << removeIt->slot_->dump() << endl;
            }
            else
                discardGop_ = -1;
        }

        if (removeIt == frames_.end())
        {
            removeIt = frames_.begin();

            LogTraceC << "discard from the front of the queue "
                      << removeIt->slot_->dump() << endl;
        }

        // check updates to codec context
        if (codecContext_.lastDecoded_ == removeIt)
            resetCodecContext();

        // check updates to any GOPs
        int32_t gopNo = removeIt->slot_->getMetaPacket()->getFrameMeta().gop_number();
        GopEntry& ge = gops_[gopNo];
        if (ge.start_ == removeIt)
            ge.start_ = frames_.end();
        if (ge.end_ == removeIt)
            ge.end_ = frames_.end();
        if (ge.end_ == ge.start_)
        {
            gops_.erase(gopNo);
            LogTraceC << "discard GOP " << gopNo << endl;
        }

        bitstreamDataPool_.push(removeIt->bitstreamData_);
        if (lastRetrieved_ == removeIt)
        {
            lockedRecycleData_ = removeIt->rawData_;
            lastRetrieved_ = frames_.end();
        }
        else
            vpxFbList_.recycle(removeIt->rawData_);
        
        if (pbPointer_ == frames_.end())
        {
            pbPointer_ = frames_.end();
            LogTraceC << "reset pb pointer" << endl;
        }

        // TODO: come back to this later
        // internally, we want slots to be const
        // however, returning slots back to client code is easier if we pass non-const
        // (since they typically will return it to the pool righ away, which requires
        // non-const-ness)
        shared_ptr<BufferSlot> s = const_pointer_cast<BufferSlot>(removeIt->slot_);
        frames_.erase(removeIt);

        LogTraceC << "discard frame " << s->dump()
                  << " total frames " << frames_.size()
                  << " pools " << bitstreamDataPool_.size()
                  << " " << vpxFbList_.getFreeBufferNum()
                  << endl;

        // TODO call it outside lock context
        onSlotDiscard(s);
    }
}

bool
DecodeQueue::frameIteratorWithinRange(const FrameQueue::iterator& it,
                                      const FrameQueue::iterator& start,
                                      const FrameQueue::iterator& end)
{
    int32_t itSampleNo = -1;
    int32_t startSampleNo = -1;
    int32_t endSampleNo = -1;

    if (it != frames_.end())
        itSampleNo = it->slot_->getNameInfo().sampleNo_;

    if (start != frames_.end())
        startSampleNo = start->slot_->getNameInfo().sampleNo_;

    if (end != frames_.end())
        endSampleNo = end->slot_->getNameInfo().sampleNo_;

    if (startSampleNo != -1)
    {
        if (endSampleNo != -1)
            return itSampleNo >= startSampleNo && itSampleNo <= endSampleNo;
        return itSampleNo >= startSampleNo;
    }

    // else
    return itSampleNo <= endSampleNo;
}

int32_t
DecodeQueue::getEncodedNum(const GopEntry& ge)
{
    if (ge.start_ == frames_.end())
    {
        if (ge.end_ != frames_.end())
        {
            int32_t nEncoded = -ge.nDecoded_;
            FrameQueue::iterator it = ge.end_;
            while (it != frames_.begin() &&
                   it->slot_->getMetaPacket()->getFrameMeta().gop_number() == ge.no_)
            {
                if (it->rawData_ == nullptr)
                    nEncoded += 1;
                    else
                        break;
                it = prev(it);
            }
            return  (nEncoded < 0 ? 0 : nEncoded);
        }
        return 0;
    }
    
    return distance(ge.start_, ge.end_) - ge.nDecoded_ + (ge.end_ == frames_.end() ? 0 : 1);
}

string
DecodeQueue::dump() const
{
    stringstream ss;
    ss << "[";

    int32_t prevGopNo = -1;
    for (auto& qe : frames_)
    {
        assert(qe.slot_);
        assert(qe.slot_->getMetaPacket());

        PacketNumber sampleNo = qe.slot_->getNameInfo().sampleNo_;
        int32_t gopNo = qe.slot_->getMetaPacket()->getFrameMeta().gop_number();
        int32_t gopPos = qe.slot_->getMetaPacket()->getFrameMeta().gop_position();
        bool isKey = (qe.slot_->getMetaPacket()->getFrameMeta().type() ==
                      FrameMeta_FrameType_Key);
        bool isDecoded = qe.rawData_ ? qe.rawData_->hasImage() : false;
        bool isPbPointer = (pbPointer_ == frames_.end()) ? false : (qe.slot_ == pbPointer_->slot_);

        if (gopNo != prevGopNo)
        {
            if (prevGopNo != -1) ss << ")";
            ss << "(" << gopNo << ":" << sampleNo
            << (isKey ? "K" : "D");
//            << (isKey ? (isDecoded ? "K" : "k") : (isDecoded ? "D" : "d"));
            prevGopNo = gopNo;
        }
//        else
        {
            if (!qe.bitstreamData_) ss << "#";
            else ss << (isDecoded ? (isPbPointer ? "*" : "+") : (isPbPointer ? "x" :"."));
        }
    }

    ss << ")]";
    return ss.str();
}

//******************************************************************************
int64_t
PlaybackQueue::Sample::timestamp() const
{
    return slot_->getHeader().publishTimestampMs_;
}

PlaybackQueue::PlaybackQueue(const ndn::Name& streamPrefix,
    const std::shared_ptr<Buffer>& buffer):
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
        std::shared_ptr<const BufferSlot> slot;

        {
            std::lock_guard<std::recursive_mutex> scopedLock(mutex_);

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
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
        observers_.push_back(observer);
    }
}

void
PlaybackQueue::detach(IPlaybackQueueObserver* observer)
{
    std::vector<IPlaybackQueueObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);
    if (it != observers_.end())
    {
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
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
PlaybackQueue::onNewRequest(const std::shared_ptr<BufferSlot>&)
{
    (*sstorage_)[Indicator::BufferReservedSize] = pendingSize();
}

void
PlaybackQueue::onNewData(const BufferReceipt& receipt)
{
    if (//receipt.slot_->getState() == BufferSlot::Ready &&
        streamPrefix_.match(receipt.slot_->getPrefix()))
    {
        std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
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
//    std::lock_guard<std::recursive_mutex> scopedLock(mutex_);
//    queue_.clear();
}
