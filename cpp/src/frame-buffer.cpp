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
#include "rtt-estimation.h"
#include "ndnrtc-debug.h"
#include "ndnrtc-namespace.h"
#include "fec.h"

using namespace std;
using namespace ndnlog;
using namespace webrtc;
using namespace ndnrtc;
using namespace fec;

#define RECORD 0
#if RECORD
#include "ndnrtc-testing.h"
using namespace ndnrtc::testing;
static EncodedFrameWriter frameWriter("received-key.nrtc");
#endif

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

    if (requestTimeUsec_ <= 0)
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
    isParity_ = false;
}

//******************************************************************************
// FrameBuffer::Slot
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::Slot::Slot(unsigned int segmentSize,
                                         bool useFec):
useFec_(useFec),
state_(StateFree),
consistency_(Inconsistent),
segmentSize_(segmentSize)
{
    reset();
}

ndnrtc::new_api::FrameBuffer::Slot::~Slot()
{
    if (slotData_)
        free(slotData_);
    
    if (fecSegmentList_)
        free(fecSegmentList_);
}

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
    bool isParityPrefix = NdnRtcNamespace::isParitySegmentPrefix(interestName);
    shared_ptr<Segment> reservedSegment = prepareFreeSegment(segmentNumber, isParityPrefix);
    
    if (packetNumber*segmentNumber >= 0 &&
        reservedSegment.get())
    {
        interest.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
        reservedSegment->interestIssued(NdnRtcUtils::blobToNonce(interest.getNonce()));
        reservedSegment->setNumber(segmentNumber);
        reservedSegment->setIsParity(isParityPrefix);
        
        // rightmost child prefix
        if (packetNumber < 0)
            rightmostSegment_ = reservedSegment;
        else
        {
            SegmentNumber segIdx = (isParityPrefix)?toMapParityIdx(segmentNumber):segmentNumber;
            activeSegments_[segIdx] = reservedSegment;
        }
        
        if (state_ == StateFree)
        {
            NdnRtcNamespace::trimDataTypeComponent(interestName, slotPrefix_);
            state_ = StateNew;
            requestTimeUsec_ = reservedSegment->getRequestTimeUsec();
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
    bool isParity = NdnRtcNamespace::isParitySegmentPrefix(interestName);
    
    if (packetNumber < 0 &&
        segmentNumber < 0 &&
        rightmostSegment_.get() != nullptr)
    {
        rightmostSegment_->markMissed();
        return RESULT_OK;
    }

    int res = RESULT_ERR;
    shared_ptr<Segment> segment = getSegment(segmentNumber, isParity);
    
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
    bool isParity = NdnRtcNamespace::isParitySegmentPrefix(dataName);

    if (!rightmostSegment_.get() &&
        packetNumber != packetSequenceNumber_)
        return StateFree;
    
    PrefixMetaInfo prefixMeta;
    
    if (RESULT_GOOD(PrefixMetaInfo::extractMetadata(dataName, prefixMeta)))
    {
        fixRightmost(packetNumber, segmentNumber, isParity);

        updateConsistencyWithMeta(packetNumber, prefixMeta);
        assert(prefixMeta.totalSegmentsNum_ == nSegmentsTotal_);
        
        SegmentData segmentData;
        
        if (RESULT_GOOD(SegmentData::segmentDataFromRaw(data.getContent().size(),
                                                        data.getContent().buf(),
                                                        segmentData)))
        {
            SegmentData::SegmentMetaInfo segmentMeta = segmentData.getMetadata();
            addData(segmentData.getSegmentData(), segmentData.getSegmentDataLength(),
                    segmentNumber, prefixMeta.totalSegmentsNum_,
                    isParity, nSegmentsParity_);

            if (segmentNumber == 0 && !isParity)
                updateConsistencyFromHeader();

            shared_ptr<Segment> segment = getActiveSegment(segmentNumber, isParity);
            segment->setDataPtr(getSegmentDataPtr(segmentData.getSegmentDataLength(),
                                                  segmentNumber, isParity));
            segment->setNumber(segmentNumber);
            segment->setPayloadSize(segmentData.getSegmentDataLength());
            segment->dataArrived(segmentMeta);
            segment->setIsParity(isParity);
            
            nSegmentsPending_--;
            if (!isParity)
                nSegmentsReady_++;
            else
                nSegmentsParityReady_++;
            recentSegment_ = segment;
            
            if (segment->isOriginal())
                hasOriginalSegments_ = true;
            
            vector<shared_ptr<Segment>> fetchedSegments =
                getSegments(Segment::StateFetched);
            
            if (nSegmentsReady_ == prefixMeta.totalSegmentsNum_)
            {
                state_ = StateReady;
                readyTimeUsec_ = segment->getArrivalTimeUsec();
            }
            else
            {
                if (fetchedSegments.size() < prefixMeta.totalSegmentsNum_)
                {
                    if (state_ == StateNew)
                        firstSegmentTimeUsec_ = segment->getArrivalTimeUsec();
                    
                    state_ = StateAssembling;
                }
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
    
    memset(fecSegmentList_, '0', nSegmentsTotal_+nSegmentsParity_);
    
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
    nSegmentsParity_ = 0;
    nSegmentsParityReady_ = 0;
    slotPrefix_ = Name();
    recentSegment_.reset();
    assembledSize_ = 0;
    nRtx_ = 0;
    hasOriginalSegments_ = false;
    isRecovered_ = false;
    requestTimeUsec_ = -1;
    readyTimeUsec_ = -1;
    firstSegmentTimeUsec_ = -1;
    memset(slotData_, 0, allocatedSize_);
    
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

int
ndnrtc::new_api::FrameBuffer::Slot::recover()
{
    int nRecovered  = 0;
    
    if (getAssembledLevel() < 1.)
    {        
        Rs28Decoder dec(nSegmentsTotal_, nSegmentsParity_, segmentSize_);
        
        if (nSegmentsParity_ > 0)
            nRecovered = dec.decode(slotData_,
                                    slotData_+nSegmentsTotal_*segmentSize_,
                                    fecSegmentList_);
        
//        LogTraceC << "recovery: "
//        << "level " << getAssembledLevel()
//        << " total " << nSegmentsTotal_
//        << " fetched " << nSegmentsReady_
//        << " parity " << nSegmentsParity_
//        << " fetched " << nSegmentsParityReady_
//        << " recovered " << nRecovered << " segments from "
//        << slotPrefix_ << endl;
    }
    
    isRecovered_ = ((nRecovered + nSegmentsReady_) >= nSegmentsTotal_);
    
    return nRecovered;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::PlaybackComparator::operator()
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot1,
 const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot2) const
{
    if (!slot1.get() || !slot2.get())
        return false;
    
    bool ascending = false;
    
    if (slot1->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta &&
        slot2->getConsistencyState() & ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta)
    {
        ascending = slot1->getPlaybackNumber() < slot2->getPlaybackNumber();
    } // end: prefix meta compare
    else
    {
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
        } // end: timestamp compare
        else
        {
            if (slot1->getNamespace() == slot2->getNamespace())
            {
                if (slot1->getConsistencyState() == slot2->getConsistencyState())
                {
                    assert(slot1->getConsistencyState() == Inconsistent);
                    assert(slot2->getConsistencyState() == Inconsistent);
                    
                    ascending = slot1->getSequentialNumber() < slot2->getSequentialNumber();
                }
                else
                    ascending = slot2->getConsistencyState()&Inconsistent;
            } // end: seq number compare
            else
            {
                if (slot1->getConsistencyState() == slot2->getConsistencyState())
                {
                    assert(slot1->getConsistencyState() == Inconsistent);
                    assert(slot2->getConsistencyState() == Inconsistent);
                    
                    // when slot1 and slot2 are both inconsistent
                    // comparison slot1 < slot2 will be true if slot1 comes from a
                    // Delta namespace, i.e. key frames are postponed to the
                    // back of queue
                    ascending = (slot1->getNamespace() == FrameBuffer::Slot::Delta);
                }
                else
                {
                    // either slot that has some consistency should go earlier
                    ascending = (slot1->getConsistencyState()&ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta);
                } // end: namespace compare
            }
        }
    }
    return ascending^inverted_;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::isNextPacket(const Slot& slot,
                                                 const Slot& nextSlot)
{
    if (slot.getConsistencyState()&Slot::HeaderMeta &&
        nextSlot.getConsistencyState()&Slot::HeaderMeta)
    {
        return (slot.getPlaybackNumber() == nextSlot.getPlaybackNumber()-1);
    }
    
    return false;
}

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>
ndnrtc::new_api::FrameBuffer::Slot::prepareFreeSegment(SegmentNumber segNo,
                                                       bool isParity)
{
    SegmentNumber segIdx = (isParity)?toMapParityIdx(segNo):segNo;
    
    shared_ptr<Segment> freeSegment(NULL);
    map<SegmentNumber, shared_ptr<Segment>>::iterator
        it = activeSegments_.find(segIdx);
    
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

        Name segmentPrefix(getPrefix());

        // check for rightmost
        if (segNo >= 0)
        {
            NdnRtcNamespace::appendDataKind(segmentPrefix, isParity);
            freeSegment->setPrefix(Name(segmentPrefix).appendSegment(segNo));
        }
        else
            freeSegment->setPrefix(Name(segmentPrefix));
    }
    
    return freeSegment;
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>&
ndnrtc::new_api::FrameBuffer::Slot::getActiveSegment(SegmentNumber segmentNumber,
                                                     bool isParity)
{
    SegmentNumber segIdx = (isParity)?toMapParityIdx(segmentNumber):segmentNumber;
    map<SegmentNumber, shared_ptr<Segment>>::iterator
    it = activeSegments_.find(segIdx);
    
    if (it != activeSegments_.end())
        return it->second;
    
    // should never happen
    assert(false);
    shared_ptr<Segment> nullSeg(NULL);
    return nullSeg;
}

void
ndnrtc::new_api::FrameBuffer::Slot::fixRightmost(PacketNumber packetNumber,
                                                 SegmentNumber segmentNumber,
                                                 bool isParity)
{
    SegmentNumber segIdx = (isParity)?toMapParityIdx(segmentNumber):segmentNumber;
    map<SegmentNumber, shared_ptr<Segment>>::iterator
    it = activeSegments_.find(segIdx);
    
    if (it == activeSegments_.end() &&
        rightmostSegment_.get())
    {
        setPrefix(slotPrefix_.append(NdnRtcUtils::componentFromInt(packetNumber)));

        Name segmentPrefix(getPrefix());
        NdnRtcNamespace::appendDataKind(segmentPrefix, isParity);

        rightmostSegment_->setPrefix(segmentPrefix.appendSegment(segmentNumber));
        
        activeSegments_[segIdx] = rightmostSegment_;
        rightmostSegment_.reset();
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::prepareStorage(unsigned int segmentSize,
                                                   unsigned int nSegments,
                                                   unsigned int nParitySegments)
{
    unsigned int slotSize = segmentSize*(nSegments+nParitySegments);
    
    if (allocatedSize_ < slotSize)
    {
        assert(assembledSize_ == 0);
        
        slotData_ = (unsigned char*)realloc(slotData_, slotSize);
        memset(slotData_, 0, slotSize);
        
        fecSegmentList_ = (unsigned char*)realloc(fecSegmentList_, nSegments+nParitySegments);
        memset(fecSegmentList_, '0', nSegments+nParitySegments);
        allocatedSize_ = slotSize;
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::addData(const unsigned char* segmentData,
                                            unsigned int segmentSize,
                                            SegmentNumber segNo,
                                            unsigned int totalSegNum,
                                            bool isParitySegment,
                                            unsigned int totalParitySegNum)
{
    prepareStorage(segmentSize_, totalSegNum, totalParitySegNum);

    unsigned int segmentIdx = (isParitySegment)? totalSegNum+segNo : segNo;
    
    memcpy(slotData_+segmentIdx*segmentSize_, segmentData, segmentSize);
    
    fecSegmentList_[segmentIdx] = '1';
    assembledSize_ += segmentSize;
}

unsigned char*
ndnrtc::new_api::FrameBuffer::Slot::getSegmentDataPtr(unsigned int segmentSize,
                                                      SegmentNumber segmentNumber,
                                                      bool isParity)
{
    unsigned int segmentIdx = (isParity)?segmentNumber+nSegmentsTotal_:segmentNumber;
        
    if (allocatedSize_ > segmentSize*segmentIdx)
    {
        return slotData_+segmentSize*segmentIdx;
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
ndnrtc::new_api::FrameBuffer::Slot::getSegment(SegmentNumber segNo,
                                               bool isParity) const
{
    SegmentNumber segIdx = (isParity)?toMapParityIdx(segNo):segNo;
    shared_ptr<Segment> segment(NULL);
    map<SegmentNumber, shared_ptr<Segment>>::const_iterator
    it = activeSegments_.find(segIdx);
    
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
    nSegmentsParity_ = prefixMeta.paritySegmentsNum_;
    
    refineActiveSegments();
    initMissingSegments();
    
    if (useFec_)
        initMissingParitySegments();
}

void
ndnrtc::new_api::FrameBuffer::Slot::refineActiveSegments()
{
    std::map<SegmentNumber, shared_ptr<Segment>>::iterator
    it = activeSegments_.begin();
    
    while (activeSegments_.size() > nSegmentsTotal_+nSegmentsParity_)
    {
        SegmentNumber segNo = it->second->getNumber();
        SegmentNumber segNum = (it->second->isParity())?nSegmentsParity_:nSegmentsTotal_;
        
        if (segNo >= segNum)
        {
            activeSegments_.erase(it++);
        }
        else
            it++;
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::initMissingSegments()
{
    SegmentNumber segNo = nSegmentsTotal_-1;
    Name segmentPrefix(getPrefix());
    NdnRtcNamespace::appendDataKind(segmentPrefix, false);
    
    bool stop = false;
    while (!stop && segNo >= 0)
    {
        if (activeSegments_.find(segNo) == activeSegments_.end())
        {
            shared_ptr<Segment> segment(new Segment());
            segment->setNumber(segNo);
            segment->markMissed();
            segment->setPrefix(Name(segmentPrefix).appendSegment(segNo));
            
            activeSegments_[segNo] = segment;
            nSegmentsMissing_++;
        }
        
        segNo--;
    }
}

void
ndnrtc::new_api::FrameBuffer::Slot::initMissingParitySegments()
{
    SegmentNumber segNo = nSegmentsParity_-1;
    Name segmentPrefix(getPrefix());
    NdnRtcNamespace::appendDataKind(segmentPrefix, true);

    bool stop = false;
    while (!stop && segNo >= 0)
    {
        SegmentNumber segIdx = toMapParityIdx(segNo);
        
        if (activeSegments_.find(segIdx) == activeSegments_.end())
        {
            shared_ptr<Segment> segment(new Segment());
            segment->setNumber(segNo);
            segment->setIsParity(true);
            segment->markMissed();
            segment->setPrefix(Name(segmentPrefix).appendSegment(segNo));
            
            activeSegments_[segIdx] = segment;
            nSegmentsMissing_++;
        }
        
        segNo--;
    }
}

bool
ndnrtc::new_api::FrameBuffer::Slot::updateConsistencyFromHeader()
{
    if (consistency_&HeaderMeta)
        return false;
    
    consistency_ |= HeaderMeta;
    
    PacketData::PacketMetadata metadata = PacketData::metadataFromRaw(allocatedSize_,
                                                                      slotData_);
    
    packetRate_ = metadata.packetRate_;
    producerTimestamp_ = metadata.timestamp_;
    
    return true;
}

std::string
ndnrtc::new_api::FrameBuffer::Slot::dump()
{
    std::stringstream dump;
    
    dump
    << (getNamespace() == Slot::Delta?"D":"K") << ", "
    << setw(7) << getSequentialNumber() << ", "
    << setw(7) << getPlaybackNumber() << ", "
    << setw(10) << getProducerTimestamp() << ", "
    << setw(3) << getAssembledLevel()*100 << "% ("
    << ((double)nSegmentsParityReady_/(double)nSegmentsParity_)*100 << "%), "
    << setw(5) << getPairedFrameNumber() << ", "
    << Slot::toString(getConsistencyState()) << ", "
    << setw(3) << getPlaybackDeadline() << ", "
    << (hasOriginalSegments_?"ORIG":"CACH") << ", "
    << setw(3) << nRtx_ << ", "
    << (isRecovered_ ? "R" : "I") << ", "
    << setw(2) << nSegmentsTotal_ << "/" << nSegmentsReady_
    << "/" << nSegmentsPending_ << "/" << nSegmentsMissing_
    << "/" << nSegmentsParity_ << " "
    << getLifetime() << " "
    << getAssemblingTime();
    
    return dump.str();
}

//******************************************************************************
// PlaybackQueue
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::PlaybackQueue::PlaybackQueue
(double playbackRate):
playbackRate_(playbackRate),
lastFrameDuration_(ceil(1000./playbackRate)),
comparator_(FrameBuffer::Slot::PlaybackComparator(false))
{
}

//******************************************************************************
#pragma mark - public
int64_t
ndnrtc::new_api::FrameBuffer::PlaybackQueue::getPlaybackDuration(bool estimate)
{
    int64_t playbackDurationMs = 0;
    
    if (this->size() >= 1)
    {
        PlaybackQueueBase::iterator it = this->begin();
        shared_ptr<FrameBuffer::Slot> slot1 = *it;
    
        while (++it != this->end())
        {
            shared_ptr<FrameBuffer::Slot> slot2 = *it;
            
            // if header metadata is available - we have frame timestamp
            // and frames are consequent - we have real playback time
            if (slot1->getConsistencyState()&Slot::HeaderMeta &&
                slot2->getConsistencyState()&Slot::HeaderMeta &&
                slot1->getPlaybackNumber() + 1 == slot2->getPlaybackNumber())
            {
                lastFrameDuration_ = (slot2->getProducerTimestamp() - slot1->getProducerTimestamp());
                assert(lastFrameDuration_ != 0);
                playbackDurationMs += lastFrameDuration_;
            }
            else
            {
                // otherwise - infer playback duration from the producer rate
                playbackDurationMs += (estimate)?getInferredFrameDuration():0;
            }
            
            slot1 = slot2;
        }
    }
    
    return playbackDurationMs;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackDeadlines()
{
    sort();
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
    assert(slot.get());
    
    this->push_back(slot);
    sort();
    updatePlaybackDeadlines();
    
    {
        LogTraceC << "▼push[" << slot->dump() << "]" << endl;
        dumpQueue();
        
        LogStatC << "▼push " << dumpShort() << endl;
    }
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::PlaybackQueue::peekSlot()
{
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
    shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> slot(NULL);
    
    if (0 != this->size())
    {
        slot = *(this->begin());
        LogTraceC << "▲pop [" << slot->dump() << "]" << endl;
        
        this->erase(this->begin());
        updatePlaybackDeadlines();
        
        dumpQueue();
        LogStatC << "▲pop " << dumpShort() << endl;
    }
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackRate(double playbackRate)
{
    playbackRate_ = playbackRate;
    
    LogTraceC << "update rate " << playbackRate << endl;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::clear()
{
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

    for (it = this->begin(); it != this->end(); ++it)
    {
        LogTraceC
        << "[" << setw(3) << i++ << ": "
        << (*it)->dump() << "]" << endl;
    }
}

std::string
ndnrtc::new_api::FrameBuffer::PlaybackQueue::dumpShort()
{
    std::stringstream ss;
    PlaybackQueueBase::iterator it;
    int nSkipped = 0;
    
    ss << "[" ;
    for (it = this->begin(); it != this->end(); ++it)
    {
        if (it+2 == this->end())
            ss << " +" << nSkipped << " ";
        
        if (it == this->begin() ||
            it+1 == this->end() || it+2 == this->end() ||
            (*it)->getNamespace() == Slot::Key)
        {
            if (nSkipped != 0 && it < this->end()-2)
            {
                ss << " +" << nSkipped << " ";
                nSkipped = 0;
            }
                
            ss << (*it)->getSequentialNumber() << "(";
            
            if (it+1 == this->end() ||
                it+2 == this->end() ||
                (*it)->getNamespace() == Slot::Key)
                ss << (*it)->getPlaybackDeadline() << "|";
            
            ss << round((*it)->getAssembledLevel()*100)/100 << ")";
        }
        else
            nSkipped++;
    }
    ss << "]";
    
    return ss.str();
}

//******************************************************************************
// FrameBuffer
//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::FrameBuffer::FrameBuffer(const shared_ptr<const ndnrtc::new_api::Consumer> &consumer):
consumer_(consumer),
state_(Invalid),
targetSizeMs_(-1),
estimatedSizeMs_(-1),
isEstimationNeeded_(true),
playbackQueue_(consumer_->getParameters().producerRate),
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
    
    LogInfoC << "initialized" << endl;
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

    LogTraceC
    << "flushed. active "
    << activeSlots_.size()
    << ". pending " << pendingEvents_.size()
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
            LogWarnC << "slot locked " << it->first << endl;
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
          LogWarnC << "error adding " << interest.getName() << endl;
    }
    else
        LogWarnC << "no free slots" << endl;

    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::interestRangeIssued(const ndn::Interest &packetInterest,
                                                  SegmentNumber startSegmentNo,
                                                  SegmentNumber endSegmentNo,
                                                  std::vector<shared_ptr<Interest>> &segmentInterests,
                                                  bool isParity)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(packetInterest.getName());
    
    if (packetNo < 0)
    {
        LogWarnC
        << "wrong prefix for range - no packet #: "
        << packetInterest.getName() << endl;
        
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
            NdnRtcNamespace::appendDataKind(segmentInterest->getName(), isParity);
            segmentInterest->getName().appendSegment(segNo);
            
            if (RESULT_GOOD(reservedSlot->addInterest(*segmentInterest)))
            {
                segmentInterests.push_back(segmentInterest);
                
                LogTraceC << "pending " << segmentInterest->getName() << endl;
            }
            else
            {
                LogWarnC << "error adding " << segmentInterest->getName() << endl;
                break;
            }
        }
        
        isEstimationNeeded_ = true;
        
        return reservedSlot->getState();
    }
    else
        LogWarnC << "no free slots" << endl;
    
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
            
            LogTraceC
            << "appended " << dataName
            << " (" << slot->getAssembledLevel()*100 << "%)"
            << "with result " << Slot::stateToString(newState) << endl;
            
            if (oldState != newState ||
                newState == Slot::StateAssembling)
            {
                if (oldConsistency != newConsistency)
                {
                    if (newConsistency&Slot::HeaderMeta)
                    {
                        updateCurrentRate(slot->getPacketRate());
                    }
                    
                    playbackQueue_.updatePlaybackDeadlines();
                    isEstimationNeeded_ = true;
                }
                
                // check for 1st segment
                if (oldState == Slot::StateNew)
                    addBufferEvent(Event::FirstSegment, slot);
                
                // check for ready event
                if (newState == Slot::StateReady)
                {
                    LogTraceC
                    << "ready " << slot->getPrefix() << endl;
                    
                    if (slot->getRtxNum() > 0)
                    {
                        nRescuedFrames_++;
                        LogStatC << "\trescued\t" << nRescuedFrames_ << endl;
                    }
                    
                    nReceivedFrames_++;
                    addBufferEvent(Event::Ready, slot);
                    
#if RECORD
                    if (slot->getNamespace() == Slot::Key)
                    {
                        NdnFrameData *frame;
                        slot->getPacketData((PacketData**)(&frame));
                        PacketData::PacketMetadata meta = frame->getMetadata();
                        
                        webrtc::EncodedImage img;
                        frame->getFrame(img);
                        frameWriter.writeFrame(img, meta);
                        
                        delete frame;
                    }
#endif
                }
                
                // track rtt value
//                if (slot->getRecentSegment()->isOriginal())
                {
                    consumer_->getRttEstimation()->
                    updateEstimation(slot->getRecentSegment()->getRoundTripDelayUsec()/1000,
                                     slot->getRecentSegment()->getMetadata().generationDelayMs_);
                    // now update target size
                    int64_t targetBufferSize = consumer_->getBufferEstimator()->getTargetSize();
                    setTargetSize(targetBufferSize);
                    
                    LogTraceC << "target buffer size updated: "
                    << targetBufferSize << endl;
                }
                
                return newState;
            }
        }
        else
            LogErrorC
            << "error appending " << data.getName()
            << " state: " << Slot::stateToString(oldState) << endl;
    }
    
    LogWarnC
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
            LogTraceC << "timeout " << slot->getPrefix() << endl;
            
            addBufferEvent(Event::Timeout, slot);
        }
        else
        {
            LogTraceC
            << "timeout error " << interest.getName()
            << " for " << slot->getPrefix() << endl;
        }
    }
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::freeSlot(const ndn::Name &prefix)
{
    shared_ptr<Slot> slot = getSlot(prefix, true/*<=for remove*/, true);
    
    if (slot.get())
    {
        if (slot->getState() != Slot::StateLocked)
        {
            if (slot->getNamespace() == Slot::Key)
            {
                nKeyFrames_--;
                
                if (nKeyFrames_ <= 0)
                {
                    nKeyFrames_ = 0;
                    addBufferEvent(Event::NeedKey, slot);
                }
            }
            
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
    
    LogTraceC << "recycled " << nRecycledSlots_ << " slots" << endl;
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
ndnrtc::new_api::FrameBuffer::acquireSlot(ndnrtc::PacketData **packetData,
                                          PacketNumber& packetNo,
                                          PacketNumber& sequencePacketNo,
                                          PacketNumber& pairedPacketNo,
                                          bool& isKey, double& assembledLevel)
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    shared_ptr<FrameBuffer::Slot> slot = playbackQueue_.peekSlot();
    
    if (slot.get())
    {
        packetNo = slot->getPlaybackNumber();
        sequencePacketNo = slot->getSequentialNumber();
        pairedPacketNo = slot->getPairedFrameNumber();
        isKey = (slot->getNamespace() == Slot::Key);
        assembledLevel = slot->getAssembledLevel();
        
        if (playbackNo_ >= 0 &&
            slot->getPlaybackNumber() >= 0 &&
            playbackNo_+1 != slot->getPlaybackNumber())
        {
            LogWarnC
            << "playback No " << playbackNo_
            << " current playback No " << slot->getPlaybackNumber() << endl;
            
            // if we got old slot - skip it
            if (slot->getPlaybackNumber() <= playbackNo_)
                skipFrame_ = true;
        }
        
        playbackSlot_ = slot;
        
        if (!skipFrame_)
        {
            playbackNo_ = slot->getPlaybackNumber();
            
            slot->lock();
            
            if (assembledLevel > 0 &&
                assembledLevel < 1.)
            {
                nIncomplete_++;
                
                LogStatC
                << "\tincomplete\t" << nIncomplete_  << "\t"
                << (isKey?"K":"D") << "\t"
                << packetNo << "\t" << endl;
                
                if (consumer_->getParameters().useFec)
                {
                    LogTraceC << "applying FEC for incomplete frame "
                    << packetNo << " " << assembledLevel << endl;
                    
                    slot->recover();
                    
                    if (slot->isRecovered())
                    {
                        assembledLevel = 1.;
                        nRecovered_++;
                        
                        LogStatC
                        << "\trecovered\t" << slot->getSequentialNumber()  << "\t"
                        << (isKey?"K":"D") << "\t"
                        << packetNo << "\t" << endl;
                    }
                }
            }
            
            slot->getPacketData(packetData);
            
            addBufferEvent(Event::Playout, slot);
            
            LogTraceC << "locked " << slot->dump()
            << " size " << ((*packetData)?(*packetData)->getLength():-1) << endl;
        } // if (!skipFrame_)
    } // if (slot.get())
    else
    {
        playbackSlot_.reset();
        
        if (!playbackQueue_.size())
        {
            LogWarnC << "empty playback queue" << endl;
        }
        else
        {
            // should never happen
            assert(false);
        }
    } // else (slot.get())
}

int
ndnrtc::new_api::FrameBuffer::releaseAcquiredSlot(bool& isInferredDuration)
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    
    isInferredDuration = true;
    
    int playbackDuration = (skipFrame_)?0:playbackQueue_.getInferredFrameDuration();
    shared_ptr<FrameBuffer::Slot> lockedSlot = playbackSlot_;
    
    if (lockedSlot.get())
    {
        playbackQueue_.popSlot();
        
        if (!skipFrame_)
        {
            shared_ptr<FrameBuffer::Slot> nextSlot = playbackQueue_.peekSlot();
            
            if (nextSlot.get() &&
                FrameBuffer::Slot::isNextPacket(*lockedSlot, *nextSlot))
            {
                playbackDuration = nextSlot->getProducerTimestamp() - lockedSlot->getProducerTimestamp();
                isInferredDuration = false;
                
                LogTraceC << "playback " << lockedSlot->getSequentialNumber()
                << " " << playbackDuration << endl;
            }
            else
            {
                LogTraceC << "playback " << lockedSlot->getSequentialNumber()
                << " (inferred) " << playbackDuration << endl;
            }
            
            LogTraceC << "unlocked " << lockedSlot->getPrefix() << endl;
            
            lockedSlot->unlock();
            isEstimationNeeded_ = true;
        }
        else
            isInferredDuration = false;
        
        freeSlot(lockedSlot->getPrefix());
        playbackSlot_.reset();
    }
    
    // reset skipFrame_ flag
    skipFrame_ = false;
    
    return playbackDuration;
}

void
ndnrtc::new_api::FrameBuffer::recycleEvent(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    addBufferEvent(event.type_, event.slot_);
}

void
ndnrtc::new_api::FrameBuffer::dump()
{
    CriticalSectionScoped scopedCs(&syncCs_);
    
    LogTraceC
    << "buffer dump (duration est " << getEstimatedBufferSize()
    << " playable " << getPlayableBufferSize() << ")" << endl;
    
    playbackQueue_.dumpQueue();
}

void
ndnrtc::new_api::FrameBuffer::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    playbackQueue_.setDescription(NdnRtcUtils::toString("%s-pqueue",
                                                        getDescription().c_str()));
}

void
ndnrtc::new_api::FrameBuffer::setLogger(ndnlog::new_api::Logger *logger)
{
    playbackQueue_.setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
ndnrtc::new_api::FrameBuffer::getStatistics(ReceiverChannelPerformance &stat)
{
    stat.nReceived_ = nReceivedFrames_;
    stat.nRescued_ = nRescuedFrames_;
    stat.nIncomplete_ = nIncomplete_;
    stat.nRecovered_ = nRecovered_;
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
        slot->setPrefix(Name(lookupPrefix));
        
        dumpActiveSlots();
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void
ndnrtc::new_api::FrameBuffer::estimateBufferSize()
{
    CriticalSectionScoped scopedCs_(&syncCs_);
    estimatedSizeMs_ = playbackQueue_.getPlaybackDuration();
}

void
ndnrtc::new_api::FrameBuffer::resetData()
{
    LogTraceC << "buffer reset" << endl;
    activeSlots_.clear();
    playbackQueue_.clear();
    
    state_ = Invalid;
    addStateChangedEvent(state_);
    
    isWaitingForRightmost_ = true;
    targetSizeMs_ = -1;
    estimatedSizeMs_ = -1;
    isEstimationNeeded_ = true;
    playbackNo_ = -1;
    skipFrame_ = false;
    playbackSlot_.reset();
    nKeyFrames_ = 0;
    nReceivedFrames_ = 0;
    nRescuedFrames_ = 0;
    nIncomplete_ = 0;
    nRecovered_ = 0;
    forcedRelease_ = false;
    
    setTargetSize(consumer_->getParameters().jitterSize);
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
            res = (NdnRtcNamespace::trimDataTypeComponent(prefix, lookupPrefix) >= 0);
        else
        {
            lookupPrefix = prefix;
            res = true;
        }
    }
    else
        res = false;
    
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
    while (freeSlots_.size() < consumer_->getParameters().bufferSize)
    {
        unsigned int payloadSegmentSize = consumer_->getParameters().segmentSize - SegmentData::getHeaderSize();
        
        shared_ptr<Slot> slot(new Slot(payloadSegmentSize,
                                       consumer_->getParameters().useFec));
        
        freeSlots_.push_back(slot);
        addBufferEvent(Event::FreeSlot, slot);
    }
    
    playbackQueue_.updatePlaybackRate(consumer_->getParameters().producerRate);
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
        
        if (NdnRtcNamespace::isKeyFramePrefix(interest.getName()))
            nKeyFrames_++;
        
        LogTraceC << "reserved " << reservedSlot->getPrefix() << endl;
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
        
        LogTraceC << "fixed righmost entry" << endl;
    }
}

void
ndnrtc::new_api::FrameBuffer::dumpActiveSlots()
{
    std::map<Name, shared_ptr<FrameBuffer::Slot>>::iterator it;
    int i = 0;
    
    LogTraceC << "=== active slots dump" << endl;
    
    for (it = activeSlots_.begin(); it != activeSlots_.end(); ++it)
    {
        LogTraceC
        << i << " "
        << it->first << " " << it->second.get() << endl;
        
        i++;
    }
}
