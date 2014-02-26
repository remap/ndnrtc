//
//  segmentizer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "segmentizer.h"
#include "ndnrtc-namespace.h"

using namespace ndnrtc;

//******************************************************************************
#pragma mark - construction/destruction
Segmentizer::SegmentData::SegmentData(const unsigned char *data,
                                      const unsigned int dataSize)
{
    unsigned int headerSize = sizeof(SegmentHeader);
    
    length_ = headerSize + dataSize;
    data_ = (unsigned char*)malloc(length_);
    
    memcpy(data_+headerSize, data, dataSize);
    
    ((SegmentHeader*)(&data_[0]))->headerMarker_ = NDNRTC_SEGHDR_MRKR;
    ((SegmentHeader*)(&data_[0]))->metaInfo_ = {0,0,0};
    ((SegmentHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_SEGBODY_MRKR;
}

Segmentizer::SegmentData::SegmentData(const unsigned char *data,
                                      const unsigned int dataSize,
                                      const SegmentMetaInfo &metaInfo):
SegmentData(data, dataSize)
{
    ((SegmentHeader*)(&data_[0]))->metaInfo_ = metaInfo;
}

//******************************************************************************
#pragma mark - public
int Segmentizer::SegmentData::unpackSegmentData(const unsigned char *segmentData,
                                                const unsigned int segmentDataSize,
                                                unsigned char **data,
                                                unsigned int &dataSize,
                                                SegmentMetaInfo &metaInfo)
{
    unsigned int headerSize = sizeof(SegmentHeader);
    
    if (segmentData &&
        segmentDataSize > headerSize &&
        ((SegmentHeader*)(&segmentData[0]))->headerMarker_ == NDNRTC_SEGHDR_MRKR &&
        ((SegmentHeader*)(&segmentData[0]))->bodyMarker_ == NDNRTC_SEGBODY_MRKR)
    {
        dataSize = segmentDataSize - headerSize;
        *data = (unsigned char*)segmentData+headerSize;
        metaInfo = ((SegmentHeader*)(&segmentData[0]))->metaInfo_;
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

void Segmentizer::SegmentData::updateMetaInfo(const SegmentMetaInfo &metaInfo)
{
    ((SegmentHeader*)(&data_[0]))->metaInfo_ = metaInfo;
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
Segmentizer::Segmentizer(const ParamsStruct &params) :
NdnRtcObject(params),
syncCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
{
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
}

Segmentizer::~Segmentizer()
{
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
}

//******************************************************************************
#pragma mark - public
int Segmentizer::init(const shared_ptr<Face> &ndnFace,
                      const shared_ptr<ndn::Transport> &transport)
{
    shared_ptr<string> userPrefix = NdnRtcNamespace::getStreamPrefix(params_);
    
    if (!userPrefix.get())
        notifyError(-1, "bad user prefix");
    
    certificateName_ = NdnRtcNamespace::certificateNameForUser(*userPrefix);
    
    ndnFace_ = ndnFace;
    ndnTransport_ = transport;
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(*userPrefix);
    
    registerPrefix();
    
    return RESULT_OK;
}

int Segmentizer::publishData(const Name &prefix,
                             const unsigned char *payload,
                             unsigned int payloadSize,
                             int freshnessSeconds)
{
    if (!ndnTransport_->getIsConnected())
        return notifyError(-1, "can't send packet - no connection");
    
    SegmentData::SegmentMetaInfo meta = {0, 0, 0};
    SegmentData segmentData(payload, payloadSize, meta);
    
    lookupPrefixInPit(prefix, meta);
    
    segmentData.updateMetaInfo(meta);
    
    Data data(prefix);
    
    data.getMetaInfo().setTimestampMilliseconds(NdnRtcUtils::timestamp());
    data.getMetaInfo().setFreshnessSeconds(freshnessSeconds);
    data.setContent(segmentData.getData(), segmentData.getLength());
    ndnKeyChain_->sign(data, *certificateName_);
    
    SignedBlob encodedData = data.wireEncode();
    ndnTransport_->send(*encodedData);
    
    NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                       data.getContent().size());
    
    TRACE("published segment %s", prefix.toUri().c_str());
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - private
void Segmentizer::registerPrefix()
{
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    if (packetPrefix.get())
    {
        ndnFace_->registerPrefix(Name(packetPrefix->c_str()),
                                      bind(&Segmentizer::onInterest,
                                           this, _1, _2, _3),
                                      bind(&Segmentizer::onRegisterFailed,
                                           this, _1));
    }
    else
        notifyError(-1, "bad packet prefix");
}

void Segmentizer::onInterest(const shared_ptr<const Name>& prefix,
                const shared_ptr<const Interest>& interest,
                ndn::Transport& transport)
{
    TRACE("adding interest %s to PIT", interest->toUri().c_str());
    
    PitEntry pitEntry;
    
    pitEntry.arrivalTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    pitEntry.interest_ = interest;
    
    pit_[(*prefix)] = pitEntry;
}

void Segmentizer::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    notifyError(-1, "registration on %s has failed", prefix->toUri().c_str());
}

void Segmentizer::lookupPrefixInPit(const ndn::Name &prefix,
                                    SegmentData::SegmentMetaInfo &metaInfo)
{
    {
        webrtc::CriticalSectionScoped scopedCs_(&syncCs_);
        
        map<Name, PitEntry>::iterator pitHit = pit_.find(prefix);
        
        if (pitHit == pit_.end())
        {
            TRACE("looking for closest in PIT");
            pitHit = pit_.upper_bound(prefix);
        }
        else
            TRACE("found pending interest for prefix %s", prefix.toUri().c_str());
        
        if (pitHit != pit_.end())
        {
            int64_t currentTime = NdnRtcUtils::millisecondTimestamp();
            
            shared_ptr<const Interest> pendingInterest = pitHit->second.interest_;
            
            metaInfo.interestNonce_ =
                NdnRtcUtils::blobToNonce(pendingInterest->getNonce());
            metaInfo.interestArrivalMs_ = pitHit->second.arrivalTimestamp_;
            metaInfo.generationDelayMs_ = (uint32_t)(currentTime - pitHit->second.arrivalTimestamp_);
        }
        else
            WARN("couldn't find pending interest for prefix");
    }
}
