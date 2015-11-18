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
#include "buffer-estimator.h"
#include "ndnrtc-debug.h"
#include "ndnrtc-namespace.h"
#include "fec.h"
#include "pipeliner.h"
#include "consumer.h"

using namespace boost;
using namespace ndnlog;
using namespace webrtc;
using namespace ndnrtc;
using namespace fec;
using namespace ndnrtc::new_api::statistics;

#define RECORD 0
#if RECORD
#include "ndnrtc-testing.h"
using namespace ndnrtc::testing;
static EncodedFrameWriter frameWriter("received-key.nrtc");
#endif

const unsigned int ndnrtc::new_api::FrameBuffer::MinRetransmissionInterval = 150;

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
    PacketNumber packetNumber;
    SegmentNumber segmentNumber;
    NdnRtcNamespace::getSegmentationNumbers(interestName, packetNumber,
                                            segmentNumber);
    
    if (packetSequenceNumber_ >= 0 && packetSequenceNumber_ != packetNumber)
        return RESULT_ERR;
    
    if (packetNumber >= 0)
        packetSequenceNumber_ = packetNumber;
    
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
    PacketNumber packetNumber;
    SegmentNumber segmentNumber;
    NdnRtcNamespace::getSegmentationNumbers(dataName, packetNumber, segmentNumber);
    
    bool isParity = NdnRtcNamespace::isParitySegmentPrefix(dataName);

    if (!rightmostSegment_.get() &&
        packetNumber != packetSequenceNumber_)
        return StateFree;
    
    if (!hasReceivedSegment(segmentNumber, isParity))
    {
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
                
                std::vector<shared_ptr<Segment> > fetchedSegments =
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
                LogError("") << "couldn't get data from segment" << std::endl;
            }
        }
        else
        {
            LogError("") << "couldn't get metadata from packet" << std::endl;
        }
    }
    else
    {
        LogWarn("") << "duplicate " << (isParity?"parity":"data") << " segment "
        << packetNumber << "-" << segmentNumber << std::endl;
    }
    
    return state_;
}

int
ndnrtc::new_api::FrameBuffer::Slot::reset()
{
    if (state_ == StateLocked)
        return RESULT_ERR;
    
    memset(fecSegmentList_, FEC_RLIST_SYMEMPTY, nSegmentsTotal_+nSegmentsParity_);
    
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
    crcValue_ = 0;
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
            std::map<SegmentNumber, shared_ptr<Segment> >::iterator it;
            
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
            rightmostSegment_->discard();
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
    double assembledLevel = getAssembledLevel();
    
    if (assembledLevel > 0. && assembledLevel < 1.)
    {        
        Rs28Decoder dec(nSegmentsTotal_, nSegmentsParity_, segmentSize_);
        
        if (nSegmentsParity_ > 0)
        {
            nRecovered = dec.decode(slotData_,
                                    slotData_+nSegmentsTotal_*segmentSize_,
                                    fecSegmentList_);
            isRecovered_ = ((nRecovered + nSegmentsReady_) >= nSegmentsTotal_);

            if (isRecovered_)
                nSegmentsReady_ = nSegmentsTotal_;
        }
    }
    
    return nRecovered;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::PlaybackComparator::operator()
(const ndnrtc::new_api::FrameBuffer::Slot* slot1,
 const ndnrtc::new_api::FrameBuffer::Slot* slot2) const
{
    bool ascending = false;
    
    // make sure current locked slots are always in the beginning of the queue
    if (slot1->getState() == Slot::StateLocked ||
        slot2->getState() == Slot::StateLocked)
    {
        ascending = (slot1->getState() == Slot::StateLocked);
    }
    else
    {
        
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
                        ascending = (slot2->getConsistencyState()&Inconsistent);
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
    
    shared_ptr<Segment> freeSegment;
    std::map<SegmentNumber, shared_ptr<Segment> >::iterator
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
    std::map<SegmentNumber, shared_ptr<Segment> >::iterator
    it = activeSegments_.find(segIdx);
    
    if (it != activeSegments_.end())
        return it->second;
    
    // should never happen
    assert(false);
    static shared_ptr<Segment> nullSeg;
    return nullSeg;
}

void
ndnrtc::new_api::FrameBuffer::Slot::fixRightmost(PacketNumber packetNumber,
                                                 SegmentNumber segmentNumber,
                                                 bool isParity)
{
    SegmentNumber segIdx = (isParity)?toMapParityIdx(segmentNumber):segmentNumber;
    std::map<SegmentNumber, shared_ptr<Segment> >::iterator
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
        memset(fecSegmentList_, FEC_RLIST_SYMEMPTY, nSegments+nParitySegments);
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
    
    fecSegmentList_[segmentIdx] = FEC_RLIST_SYMREADY;
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
    else if (NdnRtcNamespace::isDeltaFramePrefix(prefix))
        packetNamespace_ = Delta;
    else
        packetNamespace_ = Unknown;
    
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(prefix);
    packetSequenceNumber_ = packetNo;
    
    slotPrefix_ = prefix;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> >
ndnrtc::new_api::FrameBuffer::Slot::getSegments(int segmentStateMask) 
{
    std::vector<shared_ptr<Segment> > foundSegments;
    std::map<SegmentNumber, shared_ptr<Segment> >::iterator it;
    
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
    shared_ptr<Segment> segment;
    std::map<SegmentNumber, shared_ptr<Segment> >::const_iterator
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
    
    if (consistency_&Inconsistent)
        consistency_ ^= Inconsistent;    
    
    consistency_ |= PrefixMeta;
    
    packetSequenceNumber_ = sequenceNumber;
    packetPlaybackNumber_ = prefixMeta.playbackNo_;
    pairedSequenceNumber_ = prefixMeta.pairedSequenceNo_;
    nSegmentsTotal_ = prefixMeta.totalSegmentsNum_;
    nSegmentsParity_ = prefixMeta.paritySegmentsNum_;
    crcValue_ = prefixMeta.crcValue_;
    
    refineActiveSegments();
    initMissingSegments();
    
    if (useFec_)
        initMissingParitySegments();
}

void
ndnrtc::new_api::FrameBuffer::Slot::refineActiveSegments()
{
    std::map<SegmentNumber, shared_ptr<Segment> >::iterator
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
    
    if (consistency_&Inconsistent)
        consistency_ ^= Inconsistent;
    
    consistency_ |= HeaderMeta;
    
    PacketData::PacketMetadata metadata = PacketData::metadataFromRaw(allocatedSize_,
                                                                      slotData_);
    
    packetRate_ = metadata.packetRate_;
    producerTimestamp_ = metadata.timestamp_;
    
    return true;
}

bool
ndnrtc::new_api::FrameBuffer::Slot::hasReceivedSegment
(SegmentNumber segNo, bool isParity)
{
    if (consistency_ & PrefixMeta)
    {
        unsigned int segmentIdx = (isParity)? nSegmentsTotal_+segNo : segNo;
        // either received or repaired
        return (fecSegmentList_[segmentIdx] == FEC_RLIST_SYMREADY ||
                fecSegmentList_[segmentIdx] == FEC_RLIST_SYMREPAIRED);
    }
    
    return false;
}

std::string
ndnrtc::new_api::FrameBuffer::Slot::dump()
{
    std::stringstream dump;
    
    dump
    << (packetNamespace_ == Slot::Delta?"D":((packetNamespace_ == Slot::Key)?"K":"U")) << ", "
    << std::setw(7) << getSequentialNumber() << ", "
    << std::setw(7) << getPlaybackNumber() << ", "
    << std::setw(10) << getProducerTimestamp() << ", "
    << std::setw(3) << getAssembledLevel()*100 << "% ("
    << ((double)nSegmentsParityReady_/(double)nSegmentsParity_)*100 << "%), "
    << std::setw(5) << getPairedFrameNumber() << ", "
    << Slot::toString(getConsistencyState()) << ", "
    << std::setw(3) << getPlaybackDeadline() << ", "
    << (hasOriginalSegments_?"ORIG":"CACH") << ", "
    << std::setw(3) << nRtx_ << ", "
    << (isRecovered_ ? "R" : "I") << ", "
    << std::setw(2) << nSegmentsTotal_ << "/" << nSegmentsReady_
    << "/" << nSegmentsPending_ << "/" << nSegmentsMissing_
    << "/" << nSegmentsParity_ << " "
    << getLifetime() << " "
    << getAssemblingTime() << " "
    << assembledSize_ << " "
    << std::hex << this;
    
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
        Slot* slot1 = *it;
    
        while (++it != this->end())
        {
            Slot* slot2 = *it;
            
            // if header metadata is available - we have frame timestamp
            // and frames are consequent - we have real playback time
            if (slot1->getConsistencyState()&Slot::HeaderMeta &&
                slot2->getConsistencyState()&Slot::HeaderMeta &&
                slot1->getPlaybackNumber() + 1 == slot2->getPlaybackNumber())
            {
                lastFrameDuration_ = (slot2->getProducerTimestamp() - slot1->getProducerTimestamp());
                // duration can be 0 if we got RTCP and RTP packets
                assert(lastFrameDuration_ >= 0);
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
        Slot* slot1 = *it;
        
        while (++it != this->end())
        {
            slot1->setPlaybackDeadline(playbackDeadlineMs);
            Slot* slot2 = *it;
            
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
(FrameBuffer::Slot* const slot)
{
    LogTraceC << "▼push[" << slot->dump() << "]" << std::endl;
    this->push_back(slot);

    updatePlaybackDeadlines();
    dumpQueue();
}

ndnrtc::new_api::FrameBuffer::Slot*
ndnrtc::new_api::FrameBuffer::PlaybackQueue::peekSlot()
{
    return (0 != this->size()) ? *(this->begin()) : nullptr;
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::popSlot()
{
    ndnrtc::new_api::FrameBuffer::Slot* slot;
    
    if (0 != this->size())
    {
        slot = *(this->begin());
        LogTraceC << "▲pop [" << slot->dump() << "]" << std::endl;
        
        this->erase(this->begin());
        updatePlaybackDeadlines();
        
        dumpQueue();
    }
}

void
ndnrtc::new_api::FrameBuffer::PlaybackQueue::updatePlaybackRate(double playbackRate)
{
    playbackRate_ = playbackRate;
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
    if (this->logger_->getLogLevel() != NdnLoggerDetailLevelAll)
        return;
        
    PlaybackQueueBase::iterator it;
    int i = 0;

    for (it = this->begin(); it != this->end(); ++it)
    {
        LogTraceC
        << "[" << std::setw(3) << i++ << ": "
        << (*it)->dump() << "]" << std::endl;
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
ndnrtc::new_api::FrameBuffer::FrameBuffer(const shared_ptr<const ndnrtc::new_api::Consumer> &consumer,
                                          const boost::shared_ptr<statistics::StatisticsStorage>& statStorage):
StatObject(statStorage),
consumer_(consumer.get()),
state_(Invalid),
targetSizeMs_(-1),
estimatedSizeMs_(-1),
isEstimationNeeded_(true)
{
    rttFilter_ = NdnRtcUtils::setupFilter(0.05);
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
        lock_guard<recursive_mutex> scopedLock(syncMutex_);
        initialize();
    }
    
    LogInfoC << "initialized" << std::endl;
    return RESULT_OK;
}

int
ndnrtc::new_api::FrameBuffer::reset()
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    
    for (std::map<Name, shared_ptr<Slot> >::iterator it = activeSlots_.begin();
         it != activeSlots_.end(); ++it)
    {
        shared_ptr<Slot> slot = it->second;
        if (slot->getState() != Slot::StateLocked)
        {
            slot->reset();
            freeSlots_.push_back(slot);
        }
        else
            LogWarnC << "slot locked " << it->first << std::endl;
    }
    
    resetData();
    
    return RESULT_OK;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::interestIssued(ndn::Interest &interest)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    shared_ptr<Slot> reservedSlot = getSlot(interest.getName(), false);
    
    // check if slot is already reserved
    if (!reservedSlot.get())
        reservedSlot = reserveSlot(interest);
    
    if (reservedSlot.get())
    {
      if (RESULT_GOOD(reservedSlot->addInterest(interest)))
      {
          LogTraceC << "request: " << playbackQueue_.dumpShort() << std::endl;
          
          isEstimationNeeded_ = true;
          return reservedSlot->getState();
      }
      else
          LogWarnC << "error requesting " << interest.getName() << std::endl;
    }
    else
        LogErrorC << "no free slots" << std::endl;

    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::interestRangeIssued(const ndn::Interest &packetInterest,
                                                  SegmentNumber startSegmentNo,
                                                  SegmentNumber endSegmentNo,
                                                  std::vector<shared_ptr<Interest> > &segmentInterests,
                                                  bool isParity)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(packetInterest.getName());
    
    if (packetNo < 0)
    {
        LogWarnC
        << "wrong prefix for range - no packet #: "
        << packetInterest.getName() << std::endl;
        
        return Slot::StateFree;
    }
    
    if (startSegmentNo > endSegmentNo)
        return Slot::StateFree;
    
    shared_ptr<Slot> reservedSlot = getSlot(packetInterest.getName(), false);
    
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
                segmentInterests.push_back(segmentInterest);
            else
            {
                LogWarnC << "error requesting " << segmentInterest->getName() << std::endl;
                break;
            }
        }
        
        if (segmentInterests.size())
            LogTraceC << "requested range (" << segmentInterests.size()
            << "): " << playbackQueue_.dumpShort() << std::endl;
        
        isEstimationNeeded_ = true;
        return reservedSlot->getState();
    }
    else
        LogWarnC << "no free slots" << std::endl;
    
    return Slot::StateFree;
}

ndnrtc::new_api::FrameBuffer::Event
ndnrtc::new_api::FrameBuffer::newData(const ndn::Data &data)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    Event event;
    event.type_ = Event::Error;
    const Name& dataName = data.getName();
    
    if (isWaitingForRightmost_)
        fixRightmost(dataName);
    
    shared_ptr<Slot> slot = getSlot(dataName, false);
    
    if (slot.get())
    {
        Slot::State oldState = slot->getState();
        int oldConsistency = slot->getConsistencyState();
        
        if (oldState == Slot::StateNew ||
            oldState == Slot::StateAssembling)
        {
            Slot::State newState = slot->appendData(data);
            int newConsistency = slot->getConsistencyState();
            
            event.type_ = Event::Segment;
            event.slot_ = slot;
            
            // append data
            LogDebugC << "append: ["
            << slot->getSequentialNumber() << "-"
            << slot->getRecentSegment()->getNumber() << "] >> "
            << playbackQueue_.dumpShort() << std::endl;
            
            if (oldState != newState ||
                newState == Slot::StateAssembling)
            {
                if (oldConsistency != newConsistency)
                {
                    if (newConsistency&Slot::HeaderMeta)
                        updateCurrentRate(slot->getPacketRate());
                    
                    playbackQueue_.updatePlaybackDeadlines();
                    isEstimationNeeded_ = true;
                }
                
                // check for key frames
                if (Slot::PrefixMeta & slot->getConsistencyState())
                {
                    if (slot->getNamespace() == Slot::Delta &&
                        lastKeySeqNo_ < slot->getPairedFrameNumber())
                    {
                        callback_->onKeyNeeded(slot->getPairedFrameNumber());
                    }
                }
                
                // check for 1st segment
                if (oldState == Slot::StateNew)
                    event.type_ = Event::FirstSegment;
                
                // check for ready event
                if (newState == Slot::StateReady)
                {
                    // update stat
                    if (slot->getRtxNum() == 0)
                    {
                        (*statStorage_)[Indicator::AssembledNum]++;
                        if (slot->getNamespace() == Slot::Key)
                            (*statStorage_)[Indicator::AssembledKeyNum]++;
                    }
                    else
                    {
                        (*statStorage_)[Indicator::RescuedNum]++;
                        if (slot->getNamespace() == Slot::Key)
                            (*statStorage_)[Indicator::RescuedKeyNum]++;
                        
                        LogStatC << "resc"
                        << STAT_DIV << (*statStorage_)[Indicator::RescuedNum] << std::endl;
                    }
                    
                    event.type_ = Event::Ready;
                }
                
                if (slot->getRtxNum() == 0)
                {
                    consumer_->getRttEstimation()->
                    updateEstimation(slot->getRecentSegment()->getRoundTripDelayUsec()/1000,
                                     slot->getRecentSegment()->getMetadata().generationDelayMs_);
                    // now update target size
                    int64_t targetBufferSize = consumer_->getBufferEstimator()->getTargetSize();

                    if (targetSizeMs_ != targetBufferSize)
                    {
                        LogTraceC
                        << "new target buffer size "
                        << targetBufferSize << std::endl;
                    
                        setTargetSize(targetBufferSize);
                    }
                }

#if 0
                if (rateControl_.get())
#warning RTX should be per segment!
                    rateControl_->dataReceived(data, slot->getRtxNum());
#endif
                
                if (retransmissionsEnabled_)
                    checkRetransmissions();
                
                return event;
            }
        }
        else
        {
            ((oldState == Slot::StateReady) ? LogDebugC : LogErrorC)
            << "excessive data appending "
            << data.getName()
            << " slot state " << Slot::stateToString(oldState)
            << std::endl;
        }
    }
    else
        LogDebugC
        << "no reservation for " << data.getName()
        << " buffer state " << FrameBuffer::stateToString(state_) << std::endl;

    return event;
}

void
ndnrtc::new_api::FrameBuffer::interestTimeout(const ndn::Interest &interest)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    const Name& prefix = interest.getName();
    
    shared_ptr<Slot> slot = getSlot(prefix, false);
    
    if (slot.get())
    {
        if (RESULT_GOOD(slot->markMissing(interest)))
        {
        }
        else
        {
            LogTraceC
            << "timeout error " << interest.getName()
            << " for " << slot->getPrefix() << std::endl;
        }
    }
}

void
ndnrtc::new_api::FrameBuffer::purgeNewSlots(int& nDeltaPurged, int& nKeyPurged)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(syncMutex_);
    std::vector<boost::shared_ptr<Slot>> newSlots = getSlots(Slot::StateNew);
    std::vector<Slot*> slotsToErase(newSlots.size());
    
    std::transform(newSlots.begin(), newSlots.end(), slotsToErase.begin(),
                   [](boost::shared_ptr<Slot>& slot)->Slot*{ return slot.get(); });
    nDeltaPurged = 0;
    nKeyPurged = 0;
    
    for (auto slot:slotsToErase)
    {
        if (slot->getNamespace() == Slot::Key)
            nKeyPurged++;
        else
            nDeltaPurged++;
        
        freeSlot(slot->getPrefix());
    }
    
    playbackQueue_.erase(std::remove_if(playbackQueue_.begin(), playbackQueue_.end(),
                                        [&slotsToErase](Slot* slot){ return std::find(slotsToErase.begin(), slotsToErase.end(),
                                                                                      slot) != slotsToErase.end();}),
                         playbackQueue_.end());
}

ndnrtc::new_api::FrameBuffer::Slot::State
ndnrtc::new_api::FrameBuffer::freeSlot(const ndn::Name &prefix)
{
    shared_ptr<Slot> slot = getSlot(prefix, true/*<=for remove*/);
    
    if (slot.get())
    {
        if (slot->getState() != Slot::StateLocked)
        {
            if (slot->getNamespace() == Slot::Key)
            {
                nKeyFrames_--;
                
                if (nKeyFrames_ <= 0)
                    nKeyFrames_ = 0;
            }
            
            slot->reset();
            freeSlots_.push_back(slot);
            return slot->getState();
        }
    }
    
    return Slot::StateFree;
}

void
ndnrtc::new_api::FrameBuffer::setState(const ndnrtc::new_api::FrameBuffer::State &state)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    state_ = state;
}

void
ndnrtc::new_api::FrameBuffer::recycleOldSlots()
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    
    double playbackDuration = getEstimatedBufferSize();
    double targetSize = getTargetSize();
    int nRecycledSlots_ = 0;

    while (playbackDuration > getTargetSize()) {
        Slot* oldSlot = playbackQueue_.peekSlot();
        playbackQueue_.popSlot();
        
        nRecycledSlots_++;
        freeSlot(oldSlot->getPrefix());
        playbackDuration = playbackQueue_.getPlaybackDuration();
    }
    
    isEstimationNeeded_ = true;
    
    LogTraceC << "recycled " << nRecycledSlots_ << " "
    << playbackQueue_.dumpShort() << std::endl;
}

void
ndnrtc::new_api::FrameBuffer::recycleOldSlots(int nSlotsToRecycle)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);

    int nRecycledSlots_ = 0;
    
    while (nRecycledSlots_ < nSlotsToRecycle && playbackQueue_.size() != 0)
    {
        Slot* oldSlot = playbackQueue_.peekSlot();
        playbackQueue_.popSlot();
        
        nRecycledSlots_++;
        freeSlot(oldSlot->getPrefix());
    }
    
    isEstimationNeeded_ = true;
    
    LogTraceC << "recycled " << nRecycledSlots_ << " "
    << playbackQueue_.dumpShort() << std::endl;
}

int64_t
ndnrtc::new_api::FrameBuffer::getEstimatedBufferSize()
{
    if (isEstimationNeeded_)
    {
        lock_guard<recursive_mutex> scopedLock(syncMutex_);
        estimateBufferSize();
        isEstimationNeeded_ = false;
    }

    return estimatedSizeMs_;
}

int64_t
ndnrtc::new_api::FrameBuffer::getPlayableBufferSize()
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    int64_t size = playbackQueue_.getPlaybackDuration(false);
    
    statStorage_->updateIndicator(statistics::Indicator::BufferPlayableSize, size);
    LogStatC << "buf play" << STAT_DIV <<
    size << std::endl;
    
    return size;
}

void
ndnrtc::new_api::FrameBuffer::acquireSlot(ndnrtc::PacketData **packetData,
                                          PacketNumber& packetNo,
                                          PacketNumber& sequencePacketNo,
                                          PacketNumber& pairedPacketNo,
                                          bool& isKey, double& assembledLevel)
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    FrameBuffer::Slot* slotRaw = playbackQueue_.peekSlot();
    
    if (slotRaw)
    {
        shared_ptr<FrameBuffer::Slot> slot = getSlot(slotRaw->getPrefix());
        
        packetNo = slot->getPlaybackNumber();
        sequencePacketNo = slot->getSequentialNumber();
        pairedPacketNo = slot->getPairedFrameNumber();
        isKey = (slot->getNamespace() == Slot::Key);
        assembledLevel = slot->getAssembledLevel();
        
        // check for acquisition of old slots
        if (playbackNo_ >= 0 &&
            slot->getPlaybackNumber() >= 0 &&
            playbackNo_+1 != slot->getPlaybackNumber())
        {
            // if we got old slot - skip it
            if (slot->getPlaybackNumber() <= playbackNo_)
                skipFrame_ = true;
        }
        
        playbackSlot_ = slot;
        slot->lock();
        
        if (!skipFrame_)
        {
            playbackNo_ = slot->getPlaybackNumber();
            
            if (assembledLevel >= 0. &&
                assembledLevel < 1. &&
                consumer_->getGeneralParameters().useFec_)
            {
                slot->recover();
                
                if (slot->isRecovered())
                {
                    LogTraceC << "recovered [" << slot->dump() << "]" << std::endl;
                    assembledLevel = 1.;
                    
                    // update stat
                    (*statStorage_)[Indicator::RecoveredNum]++;
                    if (isKey)
                        (*statStorage_)[Indicator::RecoveredKeyNum]++;

                    LogStatC << "recover"
                    << STAT_DIV << (*statStorage_)[Indicator::RecoveredNum] << std::endl;
                }
            }
            
            if (assembledLevel < 1.)
            {
                LogTraceC << "incomplete [" << slot->dump() << "]" << std::endl;
                
                // update stat
                (*statStorage_)[Indicator::IncompleteNum]++;
                if (isKey)
                    (*statStorage_)[Indicator::IncompleteKeyNum]++;
            }
            
            slot->getPacketData(packetData);
            
            if (*packetData && slot->getAssembledLevel() == 1.)
            {
                int crc = (*packetData)->getCrcValue();
                if (crc != slot->getCrcValue())
                {
                    LogDebugC << "checksum error "
                    << slot->getPrefix() << " ("
                    << crc << " received vs "
                    << slot->getCrcValue() << " expected)"
                    << std::endl;
                }
            }
            
            // update stat
            (*statStorage_)[Indicator::AcquiredNum]++;
            if (isKey)
                (*statStorage_)[Indicator::AcquiredKeyNum]++;
            
            LogTraceC
            << "lock [" << slot->dump() << "]" << std::endl;
        } // if (!skip)
        else
        {
            // update stat
            (*statStorage_)[Indicator::DroppedNum]++;
            if (isKey)
                (*statStorage_)[Indicator::DroppedKeyNum]++;
        }
        
        int64_t playableSize = getPlayableBufferSize();
        
        ((playableSize < (int64_t)((double)getTargetSize()/2.)) ? LogWarnC : LogTraceC)
        << "playable buffer level:"
        << " duration " << playableSize
        << " frames " << playbackQueue_.size()
        << " dump " << shortDump()
        << std::endl;
        
    } // if (slot.get())
    else
    {
        playbackSlot_.reset();
        
        if (!playbackQueue_.size())
        {
            LogWarnC << "empty playback queue" << std::endl;
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
    lock_guard<recursive_mutex> scopedLock(syncMutex_);

    int playbackDuration = (skipFrame_)?0:playbackQueue_.getInferredFrameDuration();
    
    isInferredDuration = true;
    shared_ptr<FrameBuffer::Slot> lockedSlot = playbackSlot_;
    
    if (lockedSlot.get())
    {
        playbackQueue_.popSlot();
        
        // check for key frames
        if (Slot::PrefixMeta & lockedSlot->getConsistencyState())
        {
            if (lockedSlot->getNamespace() == Slot::Key &&
                lastKeySeqNo_ <= lockedSlot->getSequentialNumber())
            {
                callback_->onKeyNeeded(lockedSlot->getSequentialNumber()+1);
            }
        }
        
        if (!skipFrame_)
        {
            FrameBuffer::Slot* nextSlot = playbackQueue_.peekSlot();
            
            if (nextSlot &&
                FrameBuffer::Slot::isNextPacket(*lockedSlot, *nextSlot))
            {
                playbackDuration = nextSlot->getProducerTimestamp() - lockedSlot->getProducerTimestamp();
                isInferredDuration = false;
                
                LogTraceC << "delay "  << playbackDuration << " ["
                << lockedSlot->dump() << "]-" << std::endl;
            }
            else
            {
                LogWarnC << "delay inferred "
                << playbackDuration << " [" << lockedSlot->dump() << "]"
                << std::endl;
            }
            
            isEstimationNeeded_ = true;
        }
        else
        {
            LogWarnC << "skip old [" << lockedSlot->dump() << "]" << std::endl;
            isInferredDuration = false;
        }
        
        // cleanup buffer from old frames every frame
        frameReleaseCount_++;
        //if (frameReleaseCount_%5 == 0)
        {
            PacketNumber deltaPacketNo = (lockedSlot->getNamespace() == Slot::Key)?lockedSlot->getPairedFrameNumber():lockedSlot->getSequentialNumber();
            PacketNumber keyPacketNo = (lockedSlot->getNamespace() == Slot::Delta)?lockedSlot->getPairedFrameNumber():lockedSlot->getSequentialNumber();
            cleanBuffer(deltaPacketNo,
                        lastKeySeqNo_,
                        lockedSlot->getPlaybackNumber());
        }
        
        LogTraceC << "unlock [" << lockedSlot->dump() << "]" << std::endl;
        lockedSlot->unlock();
        freeSlot(lockedSlot->getPrefix());
        playbackSlot_.reset();
    }
    
    // reset skipFrame_ flag
    skipFrame_ = false;
    
    if (retransmissionsEnabled_)
        checkRetransmissions();
    
    return playbackDuration;
}

void
ndnrtc::new_api::FrameBuffer::dump()
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);
    
    LogTraceC
    << "buffer dump (duration est " << getEstimatedBufferSize()
    << " playable " << getPlayableBufferSize() << ")" << std::endl;
    
    playbackQueue_.dumpQueue();
}

std::string
ndnrtc::new_api::FrameBuffer::shortDump()
{
    std::stringstream ss;
    ss.precision(2);
    
    ss << "[ ";
    
    PlaybackQueueBase::iterator it;
    for (it = playbackQueue_.begin(); it != playbackQueue_.end(); ++it)
    {
        ss << (*it)->getSequentialNumber()
        << "(" << (*it)->getAssembledLevel() << ") ";
    }
    
    ss << "]";
    
    return ss.str();
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

//******************************************************************************
#pragma mark - private
shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::getSlot(const Name& prefix, bool remove)
{
    shared_ptr<Slot> slot;
    Name lookupPrefix;
    
    if (getLookupPrefix(prefix, lookupPrefix))
    {
        std::map<Name, shared_ptr<Slot> >::iterator it = activeSlots_.find(lookupPrefix);
        
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
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void
ndnrtc::new_api::FrameBuffer::estimateBufferSize()
{
    lock_guard<recursive_mutex> scopedLock(syncMutex_);

    estimatedSizeMs_ = playbackQueue_.getPlaybackDuration();
    statStorage_->updateIndicator(statistics::Indicator::BufferEstimatedSize,
                                  estimatedSizeMs_);
    LogStatC << "buf est" << STAT_DIV
    << estimatedSizeMs_ << std::endl;
}

void
ndnrtc::new_api::FrameBuffer::cleanBuffer(PacketNumber deltaPacketNo,
                                            PacketNumber keyPacketNo,
                                            PacketNumber absolutePacketNo)
{
    int nRecycledSlots = 0;
    
    PlaybackQueue::iterator it = playbackQueue_.begin();

    while (it != playbackQueue_.end())
    {
        Slot* slot = *it;
        bool erase = false;
        
        if (slot->getSequentialNumber() != -1)
        {
            erase = ((slot->getNamespace() == Slot::Delta &&
                      slot->getSequentialNumber() < deltaPacketNo) ||
                     (slot->getNamespace() == Slot::Key &&
                      slot->getSequentialNumber() < keyPacketNo));
        }
        
        if (slot->getPlaybackNumber() != -1)
            erase = (slot->getPlaybackNumber() < absolutePacketNo);
        
        if (erase)
        {
            if (callback_)
                callback_->onFrameDropped(slot->getSequentialNumber(),
                                          slot->getPlaybackNumber(),
                                          slot->getNamespace());
                
            // update stat
            (*statStorage_)[Indicator::DroppedNum]++;
            if (slot->getNamespace() == Slot::Key)
                (*statStorage_)[Indicator::DroppedKeyNum]++;
            
            nRecycledSlots++;
            it = playbackQueue_.erase(it);
            freeSlot(slot->getPrefix());
        }
        else
            it++;
    }
    
    isEstimationNeeded_ = true;
    
    LogTraceC << "purged " << nRecycledSlots << " "
    << playbackQueue_.dumpShort() << std::endl;
}

void
ndnrtc::new_api::FrameBuffer::resetData()
{
    LogTraceC << "buffer reset" << std::endl;
    activeSlots_.clear();
    playbackQueue_.clear();
    
    state_ = Invalid;
    
    isWaitingForRightmost_ = true;
    targetSizeMs_ = -1;
    estimatedSizeMs_ = -1;
    isEstimationNeeded_ = true;
    playbackNo_ = -1;
    lastKeySeqNo_ = -1;
    skipFrame_ = false;
    playbackSlot_.reset();
    nKeyFrames_ = 0;
    retransmissionsEnabled_ = false;
    frameReleaseCount_ = 0;
    
    setTargetSize(consumer_->getParameters().jitterSizeMs_);
}

bool
ndnrtc::new_api::FrameBuffer::getLookupPrefix(const Name& prefix,
                                              Name& lookupPrefix)
{
    return NdnRtcNamespace::trimmedLookupPrefix(prefix, lookupPrefix);
}

void
ndnrtc::new_api::FrameBuffer::initialize()
{
    while (freeSlots_.size() < consumer_->getParameters().bufferSlotsNum_)
    {
        unsigned int payloadSegmentSize = consumer_->getSettings().streamParams_.producerParams_.segmentSize_ - SegmentData::getHeaderSize();
        
        shared_ptr<Slot> slot(new Slot(payloadSegmentSize,
                                       consumer_->getGeneralParameters().useFec_));
        
        freeSlots_.push_back(slot);
    }
}

shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>
ndnrtc::new_api::FrameBuffer::reserveSlot(const ndn::Interest &interest)
{
    shared_ptr<Slot> reservedSlot;
    
    if (freeSlots_.size() > 0)
    {
        reservedSlot = freeSlots_.back();
        setSlot(interest.getName(), reservedSlot);
        freeSlots_.pop_back();
        playbackQueue_.pushSlot(reservedSlot.get());
        
        if (NdnRtcNamespace::isKeyFramePrefix(interest.getName()))
        {
            PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(interest.getName());
            lastKeySeqNo_ = frameNo;
            nKeyFrames_++;
        }
    }
    
    return reservedSlot;
}

std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> >
ndnrtc::new_api::FrameBuffer::getSlots(int slotStateMask) const
{
    std::vector<shared_ptr<FrameBuffer::Slot> > slots;
    std::map<Name, shared_ptr<Slot> >::const_iterator it;
    
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
        isWaitingForRightmost_ = false;
    }
}

void
ndnrtc::new_api::FrameBuffer::dumpActiveSlots()
{
    if (this->logger_->getLogLevel() != NdnLoggerDetailLevelAll)
        return;
    
    std::map<Name, shared_ptr<FrameBuffer::Slot> >::iterator it;
    int i = 0;
    
    LogTraceC << "=== active slots dump" << std::endl;
    
    for (it = activeSlots_.begin(); it != activeSlots_.end(); ++it)
    {
        LogTraceC
        << i << " "
        << it->first << " " << it->second.get() << std::endl;
        
        i++;
    }
}

void
ndnrtc::new_api::FrameBuffer::checkRetransmissions()
{
    int retransmissionDeadline = targetSizeMs_/2;
    
    if (retransmissionDeadline < consumer_->getRttEstimation()->getCurrentEstimation()/2)
    {
        retransmissionDeadline = consumer_->getRttEstimation()->getCurrentEstimation()/2;
    }
    
    PlaybackQueue::iterator it = playbackQueue_.begin();
    
    while ((*it)->getPlaybackDeadline() <= retransmissionDeadline &&
           (*it)->getLifetime() > MinRetransmissionInterval &&
           it != playbackQueue_.end()) {
        if ((*it)->getAssembledLevel() < 1.)
        {
            // try to recover first
            (*it)->recover();
            if (!(*it)->isRecovered())
            {
                if ((*it)->getRtxNum() == 0 &&
                    callback_)
                    callback_->onRetransmissionNeeded(*it);
            }
            else
            {
                // update stat
                (*statStorage_)[Indicator::RecoveredNum]++;
                if (((*it)->getNamespace() == Slot::Key))
                    (*statStorage_)[Indicator::RecoveredKeyNum]++;
                
                LogTraceC << "recovered [" << (*it)->dump() << "]" << std::endl;
                LogStatC << "recover"
                << STAT_DIV << (*statStorage_)[Indicator::RecoveredNum] << std::endl;
            }
        }
        it++;
    }
}
