//
//  frame-data.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <cmath>
#include <stdexcept>

#include "ndnrtc-common.h"
#include "frame-data.h"
#include "fec.h"

#define PREFIX_META_NCOMP 5

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

#if 0
//******************************************************************************
const std::string PrefixMetaInfo::SyncListMarker = "sl";
const PrefixMetaInfo PrefixMetaInfo::ZeroMetaInfo;

PrefixMetaInfo::_PrefixMetaInfo():totalSegmentsNum_(0),
playbackNo_(0),
pairedSequenceNo_(0),
paritySegmentsNum_(0),
crcValue_(0)
{}

Name
PrefixMetaInfo::toName(const PrefixMetaInfo &meta)
{
    Name metaSuffix("");
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.totalSegmentsNum_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.playbackNo_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.pairedSequenceNo_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.paritySegmentsNum_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.crcValue_));
    
    if (meta.syncList_.size())
        metaSuffix.append(SyncListMarker);
        
    for (auto pair:meta.syncList_)
    {
        metaSuffix.append(pair.first);
        metaSuffix.append(NdnRtcUtils::componentFromInt(pair.second));
    }
    
    return metaSuffix;
}

int
PrefixMetaInfo::extractMetadata(const ndn::Name &prefix,
                                PrefixMetaInfo &meta)
{
    if (prefix.size() >= PREFIX_META_NCOMP)
    {
        int slPos = 0, slAdj = 0;
        if ((slPos = NdnRtcNamespace::findComponent(prefix, SyncListMarker)) > 0)
        {
            meta.syncList_ = extractSyncList(prefix, slPos);
            slAdj = meta.syncList_.size()*2+1;
        }
            
        meta.totalSegmentsNum_ = NdnRtcUtils::intFromComponent(prefix[0-(PREFIX_META_NCOMP+slAdj)]);
        meta.playbackNo_ = NdnRtcUtils::intFromComponent(prefix[1-(PREFIX_META_NCOMP+slAdj)]);
        meta.pairedSequenceNo_ = NdnRtcUtils::intFromComponent(prefix[2-(PREFIX_META_NCOMP+slAdj)]);
        meta.paritySegmentsNum_ = NdnRtcUtils::intFromComponent(prefix[3-(PREFIX_META_NCOMP+slAdj)]);
        meta.crcValue_ = NdnRtcUtils::intFromComponent(prefix[4-(PREFIX_META_NCOMP+slAdj)]);
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

ThreadSyncList PrefixMetaInfo::extractSyncList(const ndn::Name& prefix, int markerPos)
{
    assert(markerPos < prefix.size());
    
    if (prefix[markerPos].toEscapedString() == SyncListMarker)
    {
        ThreadSyncList slList;
        
        int p = markerPos+1;
        while (p+1 < prefix.size())
        {
            slList.push_back(pair<string, PacketNumber>(prefix[p].toEscapedString(),
                                                        NdnRtcUtils::intFromComponent(prefix[p+1])));
            p += 2;
        }
        
        return slList;
    }
    
    return ThreadSyncList();
}
#endif

//******************************************************************************
NetworkData::NetworkData(unsigned int dataLength, const unsigned char* rawData):
isValid_(true)
{
    copyFromRaw(dataLength, rawData);
}

NetworkData::NetworkData(const std::vector<uint8_t>& data):
isValid_(true)
{
    data_ = data;
}

NetworkData::NetworkData(const NetworkData& networkData)
{
    data_ = networkData.data_;
    isValid_ = networkData.isValid();
}

NetworkData::NetworkData(NetworkData&& networkData):
isValid_(networkData.isValid())
{
    data_.swap(networkData.data_);
    networkData.isValid_ = false;
}

NetworkData::NetworkData(std::vector<uint8_t>& data):
data_(boost::move(data)), isValid_(true)
{
}

NetworkData& NetworkData::operator=(const NetworkData& networkData)
{
    if (this != &networkData)
    {
        data_ = networkData.data_;
        isValid_ = networkData.isValid_;
    }

    return *this;
}

void NetworkData::swap(NetworkData& networkData)
{
    std::swap(isValid_, networkData.isValid_);
    data_.swap(networkData.data_);
}

void NetworkData::copyFromRaw(unsigned int dataLength, const uint8_t* rawData)
{
    data_.assign(rawData, rawData+dataLength);
}

//******************************************************************************
DataPacket::Blob::Blob(const std::vector<uint8_t>::const_iterator& begin, 
                    const std::vector<uint8_t>::const_iterator& end):
begin_(begin), end_(end)
{
}

DataPacket::Blob& DataPacket::Blob::operator=(const DataPacket::Blob& b)
{
    if (this != &b)
    {
        begin_ = b.begin_;
        end_ = b.end_;
    }

    return *this;
}

size_t DataPacket::Blob::size() const
{
    return (end_-begin_);
}

uint8_t DataPacket::Blob::operator[](size_t pos) const
{
    return *(begin_+pos);
}

const uint8_t* DataPacket::Blob::data() const
{
    return &(*begin_);
}

//******************************************************************************
DataPacket::DataPacket(unsigned int dataLength, const uint8_t* payload):
NetworkData(dataLength, payload)
{
    data_.insert(data_.begin(), 0);
    payloadBegin_ = ++data_.begin();
}

DataPacket::DataPacket(const std::vector<uint8_t>& payload):
NetworkData(payload)
{
    data_.insert(data_.begin(), 0);
    payloadBegin_ = ++data_.begin();
}

DataPacket::DataPacket(const DataPacket& dataPacket):
NetworkData(dataPacket.data_)
{
    reinit();
}

DataPacket::DataPacket(NetworkData&& networkData):
NetworkData(boost::move(networkData))
{
    reinit();
}

const DataPacket::Blob DataPacket::getPayload() const
{
    return Blob(payloadBegin_, data_.end());
}

void DataPacket::addBlob(uint16_t dataLength, const uint8_t* data)
{
    if (dataLength == 0) return;

    // increase blob counter
    data_[0]++;
    // save blob size
    uint8_t b1 = dataLength&0x00ff, b2 = (dataLength&0xff00)>>8;
    payloadBegin_ = data_.insert(payloadBegin_, b1);
    payloadBegin_++;
    payloadBegin_ = data_.insert(payloadBegin_, b2);
    payloadBegin_++;
    // insert blob
    data_.insert(payloadBegin_, data, data+dataLength);
    reinit();
}

size_t DataPacket::wireLength(size_t payloadLength, size_t blobLength)
{
    size_t wireLength = 1+payloadLength;
    if (blobLength > 0) wireLength += 2+blobLength;
    return wireLength;
}

size_t DataPacket::wireLength(size_t payloadLength, 
    std::vector<size_t> blobLengths)
{
    size_t wireLength = 1+payloadLength;
    for (auto b:blobLengths) if (b>0) wireLength += 2+b;
    return wireLength;
}

size_t DataPacket::wireLength(size_t blobLength)
{
    if (blobLength) return blobLength+2;
    return 0;
}

size_t DataPacket::wireLength(std::vector<size_t>  blobLengths)
{
    size_t wireLength = 0;
    for (auto b:blobLengths) wireLength += 2+b;
    return wireLength;
}

void DataPacket::reinit()
{
    blobs_.clear();
    if (!data_.size()) { isValid_ = false; return; }

    std::vector<uint8_t>::iterator p1 = (data_.begin()+1), p2;
    uint8_t nBlobs = data_[0];
    bool invalid = false;

    for (int i = 0; i < nBlobs; i++)
    {
        uint8_t b1 = *p1++, b2 = *p1;
        uint16_t blobSize = b1|((uint16_t)b2)<<8;

        if (p1-data_.begin()+blobSize > data_.size())
        {
            invalid = true;
            break;
        }

        p2 = ++p1+blobSize;
        blobs_.push_back(Blob(p1,p2));
        p1 = p2;
    }
    
    if (!invalid) payloadBegin_ = p1;
    else isValid_ = false;
}

//******************************************************************************
VideoFramePacket::VideoFramePacket(const webrtc::EncodedImage& frame):
SamplePacket(frame._length, frame._buffer), isSyncListSet_(false)
{
    Header hdr;
    hdr.encodedWidth_ = frame._encodedWidth;
    hdr.encodedHeight_ = frame._encodedHeight;
    hdr.timestamp_ = frame._timeStamp;
    hdr.capture_time_ms_ = frame.capture_time_ms_;
    hdr.frameType_ = frame._frameType;
    hdr.completeFrame_ = frame._completeFrame;
    addBlob(sizeof(hdr), (uint8_t*)&hdr);
}

VideoFramePacket::VideoFramePacket(NetworkData&& networkData):
CommonSamplePacket(boost::move(networkData))
{}

const webrtc::EncodedImage& VideoFramePacket::getFrame()
{
    Header *hdr = (Header*)blobs_[0].data();
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_, 
        hdr->encodedHeight_);
    frame_ = webrtc::EncodedImage((uint8_t*)(data_.data()+(payloadBegin_-data_.begin())), getPayload().size(), size);
    frame_._encodedWidth = hdr->encodedWidth_;
    frame_._encodedHeight = hdr->encodedHeight_;
    frame_._timeStamp = hdr->timestamp_;
    frame_.capture_time_ms_ = hdr->capture_time_ms_;
    frame_._frameType = hdr->frameType_;
    frame_._completeFrame = hdr->completeFrame_;

    return frame_;
}

boost::shared_ptr<NetworkData>
VideoFramePacket::getParityData(size_t segmentLength, double ratio)
{
    if (!isValid_)
        throw std::runtime_error("Can't compute FEC parity data on invalid packet");

    size_t nDataSegmets = getLength()/segmentLength + (getLength()%segmentLength?1:0);
    size_t nParitySegments = ceil(ratio*nDataSegmets);
    if (nParitySegments == 0) nParitySegments = 1;
    
    std::vector<uint8_t> fecData(nParitySegments*segmentLength, 0);
    fec::Rs28Encoder enc(nDataSegmets, nParitySegments, segmentLength);
    size_t padding =  (nDataSegmets*segmentLength - getLength());
    boost::shared_ptr<NetworkData> parityData;

    // expand data with zeros
    data_.resize(nDataSegmets*segmentLength, 0);
    if (enc.encode(data_.data(), fecData.data()) >= 0)
        parityData = boost::make_shared<NetworkData>(boost::move(fecData));
    // shrink data back
    data_.resize(getLength()-padding);

    return parityData;
}

void
VideoFramePacket::setSyncList(const std::map<std::string, PacketNumber>& syncList)
{
    if (isHeaderSet()) throw std::runtime_error("Can't add more data to this packet"
        " as header has been set already");
    if (isSyncListSet_) throw std::runtime_error("Sync list has been already set");

    for (auto it:syncList)
    {
        addBlob(it.first.size(), (uint8_t*)it.first.c_str());
        addBlob(sizeof(it.second), (uint8_t*)&it.second);
    }

    isSyncListSet_ = true;
}

const std::map<std::string, PacketNumber> 
VideoFramePacket::getSyncList() const
{
    std::map<std::string, PacketNumber> syncList;

    for (std::vector<Blob>::const_iterator blob = blobs_.begin()+1; blob+1 < blobs_.end(); blob+=2)
    {
        syncList[std::string((const char*)blob->data(), blob->size())] = *(PacketNumber*)(blob+1)->data();
    }

    return boost::move(syncList);
}

//******************************************************************************
AudioBundlePacket::AudioSampleBlob::
AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
               const std::vector<uint8_t>::const_iterator& end):
Blob(begin, end), fromBlob_(true)
{
    header_ = *(AudioSampleHeader*)Blob::data();
}

size_t AudioBundlePacket::AudioSampleBlob::size() const
{ 
    return Blob::size()+(fromBlob_?0:sizeof(AudioSampleHeader)); 
}

size_t AudioBundlePacket::AudioSampleBlob::wireLength(size_t payloadLength)
{
    return payloadLength+sizeof(AudioSampleHeader); 
}

AudioBundlePacket::AudioBundlePacket(size_t wireLength):
CommonSamplePacket(std::vector<uint8_t>()),
wireLength_(wireLength)
{
    clear();
}

AudioBundlePacket::AudioBundlePacket(AudioBundlePacket&& bundle):
CommonSamplePacket(boost::move(std::vector<uint8_t>())),
wireLength_(0),
remainingSpace_(0)
{
    swap(boost::move(bundle));
}

bool 
AudioBundlePacket::hasSpace(const AudioBundlePacket::AudioSampleBlob& sampleBlob) const
{
    return ((long)remainingSpace_ - (long)DataPacket::wireLength(sampleBlob.size())) >= 0;
}

AudioBundlePacket& 
AudioBundlePacket::operator<<(const AudioBundlePacket::AudioSampleBlob& sampleBlob)
{
    if (hasSpace(sampleBlob))
    {
        data_[0]++;

        uint8_t b1 = sampleBlob.size()&0x00ff, b2 = (sampleBlob.size()&0xff00)>>8; 
        payloadBegin_ = data_.insert(payloadBegin_, b1);
        payloadBegin_++;
        payloadBegin_ = data_.insert(payloadBegin_, b2);
        payloadBegin_++;
        for (int i = 0; i < sizeof(sampleBlob.getHeader()); ++i)
        {
            payloadBegin_ = data_.insert(payloadBegin_, ((uint8_t*)&sampleBlob.getHeader())[i]);
            payloadBegin_++;
        }
        // insert blob
        data_.insert(payloadBegin_, sampleBlob.data(), 
            sampleBlob.data()+(sampleBlob.size()-sizeof(sampleBlob.getHeader())));
        reinit();
        remainingSpace_ -= DataPacket::wireLength(sampleBlob.size());
    }

    return *this;
}

void AudioBundlePacket::clear()
{
    data_.clear();
    data_.insert(data_.begin(),0);
    payloadBegin_ = data_.begin()+1;
    blobs_.clear();
    remainingSpace_ = AudioBundlePacket::payloadLength(wireLength_);
}

size_t AudioBundlePacket::getSamplesNum()
{
    return blobs_.size() - isHeaderSet();
}

void AudioBundlePacket::swap(AudioBundlePacket&& bundle)
{ 
    CommonSamplePacket::swap(bundle);
    std::swap(wireLength_, bundle.wireLength_);
    std::swap(remainingSpace_, bundle.remainingSpace_);
}

const AudioBundlePacket::AudioSampleBlob 
AudioBundlePacket::operator[](size_t pos) const
{
    return AudioSampleBlob(blobs_[pos].begin(), blobs_[pos].end());
}

size_t AudioBundlePacket::wireLength(size_t wireLength, size_t sampleSize)
{
    size_t sampleWireLength = AudioSampleBlob::wireLength(sampleSize);
    size_t nSamples = AudioBundlePacket::payloadLength(wireLength)/DataPacket::wireLength(sampleWireLength);
    std::vector<size_t> sampleSizes(nSamples, sampleWireLength);
    sampleSizes.push_back(sizeof(CommonHeader));

    return DataPacket::wireLength(0, sampleSizes);
}

size_t AudioBundlePacket::payloadLength(size_t wireLength)
{
    long payloadLength = wireLength-1-DataPacket::wireLength(sizeof(CommonHeader));
    return (payloadLength > 0 ? payloadLength : 0);
}

//******************************************************************************
AudioThreadMeta::AudioThreadMeta(double rate, const std::string& codec):
DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(rate), (uint8_t*)&rate);
    if (codec.size()) addBlob(codec.size(), (uint8_t*)codec.c_str());
    else isValid_ = false;
}

AudioThreadMeta::AudioThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 2 && blobs_[0].size() == sizeof(double));
}

double 
AudioThreadMeta::getRate() const
{
    return *(const double*)blobs_[0].data();
}

std::string
AudioThreadMeta::getCodec() const
{
    return std::string((const char*)blobs_[1].data(), blobs_[1].size());
}

//******************************************************************************
VideoThreadMeta::VideoThreadMeta(double rate, const FrameSegmentsInfo& segInfo,
    const VideoCoderParams& coder):
DataPacket(std::vector<uint8_t>())
{
    Meta m({rate, coder.gop_, coder.startBitrate_, coder.encodeWidth_, coder.encodeHeight_, 
        segInfo.deltaAvgSegNum_, segInfo.deltaAvgParitySegNum_,
        segInfo.keyAvgSegNum_, segInfo.keyAvgParitySegNum_});
    addBlob(sizeof(m), (uint8_t*)&m);
}

VideoThreadMeta::VideoThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 1 && blobs_[0].size() == sizeof(Meta));
}

double VideoThreadMeta::getRate() const
{
    return ((Meta*)blobs_[0].data())->rate_;
}

FrameSegmentsInfo VideoThreadMeta::getSegInfo() const
{
    Meta *m = (Meta*)blobs_[0].data();
    return FrameSegmentsInfo({m->deltaAvgSegNum_, m->deltaAvgParitySegNum_, 
        m->keyAvgSegNum_, m->keyAvgParitySegNum_});
}

VideoCoderParams VideoThreadMeta::getCoderParams() const
{
    Meta *m = (Meta*)blobs_[0].data();
    VideoCoderParams c;
    c.gop_ = m->gop_;
    c.startBitrate_ = m->bitrate_;
    c.encodeWidth_ = m->width_;
    c.encodeHeight_ = m->height_;
    return c;
}

//******************************************************************************
#define SYNC_MARKER "sync:"
MediaStreamMeta::MediaStreamMeta():
DataPacket(std::vector<uint8_t>())
{}

MediaStreamMeta::MediaStreamMeta(std::vector<std::string> threads):
DataPacket(std::vector<uint8_t>())
{
    for (auto t:threads) addThread(t);
}

void
MediaStreamMeta::addThread(const std::string& thread)
{
     addBlob(thread.size(), (uint8_t*)thread.c_str());
}

void 
MediaStreamMeta::addSyncStream(const std::string& stream)
{
    addThread("sync:"+stream);
}

std::vector<std::string>
MediaStreamMeta::getSyncStreams() const
{
    std::vector<std::string> syncStreams;
    for (auto b:blobs_)
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        size_t p = thread.find(SYNC_MARKER);
        if (p != std::string::npos)
            syncStreams.push_back(std::string(thread.begin()+sizeof(SYNC_MARKER)-1, 
                thread.end()));
            
    }
    return syncStreams;
}

std::vector<std::string>
MediaStreamMeta::getThreads() const 
{
    std::vector<std::string> threads;
    for (auto b:blobs_) 
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        if (thread.find("sync:") == std::string::npos)
            threads.push_back(thread);
    }
    return threads;
}


#if 0
//******************************************************************************
const PacketData::PacketMetadata PacketData::ZeroMetadata = {0, 0, 0};
const PacketData::PacketMetadata PacketData::BadMetadata = {-1, -1, -1};

#pragma mark - public
ndnrtc::PacketData::PacketMetadata
PacketData::getMetadata()
{
    PacketMetadata meta;
    meta.packetRate_ = -1;
    meta.timestamp_ = -1;
    
    return meta;
}

int
ndnrtc::PacketData::packetFromRaw(unsigned int length,
                                  unsigned char *data,
                                  ndnrtc::PacketData **packetData)
{
    if (NdnFrameData::isValidHeader(length, data))
    {
        NdnFrameData *frameData = new NdnFrameData();
        
        frameData->length_ = length;
        frameData->data_ = data;
        
        if (RESULT_GOOD(frameData->initFromRawData(frameData->length_,
                                                   frameData->data_)))
        {
            frameData->isValid_ = true;
            *packetData = frameData;
            return RESULT_OK;
        }
        
        delete frameData;
    }

    if (NdnAudioData::isValidHeader(length, data))
    {
        NdnAudioData *audioData = new NdnAudioData();

        audioData->length_ = length;
        audioData->data_ = data;
        
        if (RESULT_GOOD(audioData->initFromRawData(audioData->length_,
                                                   audioData->data_)))
        {
            audioData->isValid_ = true;
            *packetData = audioData;
            return RESULT_OK;
        }
        
        delete audioData;
    }

    
    return RESULT_ERR;
}

PacketData::PacketMetadata
ndnrtc::PacketData::metadataFromRaw(unsigned int length,
                                    const unsigned char *data)
{
    if (NdnFrameData::isValidHeader(length, data))
        return NdnFrameData::metadataFromRaw(length, data);
    
    if (NdnAudioData::isValidHeader(length, data))
        return NdnAudioData::metadataFromRaw(length, data);
    
    return BadMetadata;
}

//******************************************************************************
#pragma mark - construction/destruction
SegmentData::SegmentData(const unsigned char *segmentData,
                         const unsigned int dataSize,
                         SegmentMetaInfo metadata)
{
    unsigned int hdr->ize = sizeof(SegmentHeader);
    
    length_ = hdr->ize + dataSize;
    
    isValid_ = true;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(length_);
    memcpy(data_ hdr->ize, segmentData, dataSize);
    
    ((SegmentHeader*)(&data_[0]))- hdr->arker_ = NDNRTC_SEGHDR_MRKR;
    ((SegmentHeader*)(&data_[0]))->metaInfo_ = metadata;
    ((SegmentHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_SEGBODY_MRKR;
}

//******************************************************************************
#pragma mark - public
int SegmentData::initFromRawData(unsigned int dataLength,
                                 const unsigned char* rawData)
{
    if (rawData &&
        dataLength > getHeaderSize() &&
        ((SegmentHeader*)(&rawData[0]))- hdr->arker_ == NDNRTC_SEGHDR_MRKR &&
        ((SegmentHeader*)(&rawData[0]))->bodyMarker_ == NDNRTC_SEGBODY_MRKR)
    {
        isValid_ = true;
        data_ = const_cast<unsigned char*>(rawData);
        length_ = dataLength;
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

int SegmentData::segmentDataFromRaw(unsigned int dataLength,
                                    const unsigned char *rawData,
                                    ndnrtc::SegmentData &segmentData)
{
    return segmentData.initFromRawData(dataLength, rawData);
}

#if 0
//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
FrameParityData::FrameParityData(unsigned int length,
                                 const unsigned char* rawData):
PacketData(length, rawData)
{
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}

int
FrameParityData::initFromPacketData(const PacketData& packetData,
                                    double parityRatio,
                                    unsigned int nSegments,
                                    unsigned int segmentLength)
{
    uint32_t nParitySegments = getParitySegmentsNum(nSegments, parityRatio);
    
    length_ = getParityDataLength(nSegments, parityRatio, segmentLength);
    data_ = (unsigned char*)malloc(length_);
    isDataCopied_ = true;
    
    // create redundancy data
    Rs28Encoder enc(nSegments, nParitySegments, segmentLength);
    
    if (enc.encode(packetData.getData(), data_) < 0)
        return RESULT_ERR;
    else
    {
        isValid_ = true;
    }
    
    return RESULT_OK;
}

int
FrameParityData::initFromRawData(unsigned int dataLength,
                                 const unsigned char *rawData)
{
    return RESULT_OK;
}

unsigned int
FrameParityData::getParitySegmentsNum
(unsigned int nSegments, double parityRatio)
{
    return (uint32_t)ceil(parityRatio*nSegments);
}

unsigned int
FrameParityData::getParityDataLength
(unsigned int nSegments, double parityRatio, unsigned int segmentLength)
{
    return getParitySegmentsNum(nSegments, parityRatio) * segmentLength;
}
#endif
//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnFrameData::NdnFrameData(unsigned int length, const unsigned char* rawData):
PacketData(length, rawData)
{
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}
NdnFrameData::NdnFrameData(const EncodedImage &frame,
                           unsigned int segmentLength)
{
    initialize(frame, segmentLength);
}

NdnFrameData::NdnFrameData(const EncodedImage &frame, unsigned int segmentLength,
                           PacketMetadata &metadata)
{
    initialize(frame, segmentLength);
    ((FrameDataHeader*)(&data_[0]))->metadata_ = metadata;
}

NdnFrameData::~NdnFrameData()
{
}

//******************************************************************************
#pragma mark - public
int
NdnFrameData::getFrame(webrtc::EncodedImage &frame)
{
    if (!isValid())
        return RESULT_ERR;
    
    frame = frame_;
    
    return RESULT_OK;
}

void
copyFrame(const webrtc::EncodedImage &frameOriginal,
          webrtc::EncodedImage &frameCopy)
{
    
}

PacketData::PacketMetadata
NdnFrameData::getMetadata()
{
    PacketMetadata meta = PacketData::getMetadata();
    
    if (isValid())
        meta = getHeader().metadata_;
    
    return meta;
}

void
NdnFrameData::setMetadata(PacketMetadata &metadata)
{
    if (isValid())
        ((FrameDataHeader*)(&data_[0]))->metadata_ = metadata;
}

bool
NdnFrameData::isValidHeader(unsigned int length, const unsigned char *data)
{
    unsigned int hdr->ize = sizeof(FrameDataHeader);
    
    if (length >= hdr->ize)
    {
        FrameDataHeader hdr->= *((FrameDataHeader*)(&data[0]));

        if  hdr->arker_ == NDNRTC_FRAMEHDR_MRKR &&
            hdr->bodyMarker_ == NDNRTC_FRAMEBODY_MRKR)
            return true;
    }
    
    return false;
}

PacketData::PacketMetadata
NdnFrameData::metadataFromRaw(unsigned int length, const unsigned char *data)
{
    if (NdnFrameData::isValidHeader(length, data))
    {
        NdnFrameData::FrameDataHeader hdr->= *((FrameDataHeader*)data);
        return hdr->metadata_;
    }
    
    return PacketData::BadMetadata;
}

//******************************************************************************
#pragma mark - private
void
NdnFrameData::initialize(const webrtc::EncodedImage &frame, unsigned int segmentLength)
{
    unsigned int hdr->ize = sizeof(FrameDataHeader);
    unsigned int allocSize = (unsigned int)ceil((double)(frame._length hdr->ize)/(double)segmentLength)*segmentLength;
    
    length_ = frame._length hdr->ize;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(allocSize);
    memset(data_, 0, allocSize);
    
    // copy frame data with offset of hdr->    memcpy(data_ hdr->ize, frame._buffer, frame._length);
    
    // setup hdr->    ((FrameDataHeader*)(&data_[0]))- hdr->arker_ = NDNRTC_FRAMEHDR_MRKR;
    ((FrameDataHeader*)(&data_[0]))->encodedWidth_ = frame._encodedWidth;
    ((FrameDataHeader*)(&data_[0]))->encodedHeight_ = frame._encodedHeight;
    ((FrameDataHeader*)(&data_[0]))->timeStamp_ = frame._timeStamp;
    ((FrameDataHeader*)(&data_[0]))->capture_time_ms_ = frame.capture_time_ms_;
    ((FrameDataHeader*)(&data_[0]))->frameType_ = frame._frameType;
    ((FrameDataHeader*)(&data_[0]))->completeFrame_ = frame._completeFrame;
    ((FrameDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
    
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}

int
NdnFrameData::initFromRawData(unsigned int dataLength,
                              const unsigned char *rawData)
{
    unsigned int hdr->ize = sizeof(FrameDataHeader);
    FrameDataHeader hdr->= *((FrameDataHeader*)(&rawData[0]));
    
    // check markers
    if ( hdr->arker_ != NDNRTC_FRAMEHDR_MRKR ||
         hdr->bodyMarker_ != NDNRTC_FRAMEBODY_MRKR) ||
        dataLength < hdr->ize)
        return RESULT_ERR;
    
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_,
                                          hdr->encodedHeight_);
        
    frame_ = webrtc::EncodedImage(const_cast<uint8_t*>(&rawData hdr->ize]),
                                  dataLength hdr->ize, size);
    frame_._encodedWidth = hdr->encodedWidth_;
    frame_._encodedHeight = hdr->encodedHeight_;
    frame_._timeStamp = hdr->timeStamp_;
    frame_.capture_time_ms_ = hdr->capture_time_ms_;
    frame_._frameType = hdr->frameType_;
    frame_._completeFrame = hdr->completeFrame_;

    return RESULT_OK;
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnAudioData::NdnAudioData(unsigned int dataLength, const unsigned char* rawData):
PacketData(dataLength, rawData)
{
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}

//NdnAudioData::NdnAudioData(const NdnAudioData& audioData)
//{
//    isDataCopied_ = true;
//    isValid_ = audioData.getIs
//}

//******************************************************************************
#pragma mark - public
PacketData::PacketMetadata
NdnAudioData::getMetadata()
{
    PacketMetadata meta = PacketData::getMetadata();
    
    if (isValid())
        meta = getHeader().metadata_;
    
    return meta;
}

void
NdnAudioData::setMetadata(PacketMetadata &metadata)
{
    if (isValid())
        ((AudioDataHeader*)(&data_[0]))->metadata_ = metadata;
}

bool
NdnAudioData::isValidHeader(unsigned int length, const unsigned char *data)
{
    unsigned int hdr->ize = sizeof(AudioDataHeader);
    
    if (length >= hdr->ize)
    {
        AudioDataHeader hdr->= *((AudioDataHeader*)(&data[0]));
        
        if  hdr->arker_ == NDNRTC_AUDIOHDR_MRKR &&
            hdr->bodyMarker_ == NDNRTC_AUDIOBODY_MRKR)
            return true;
    }
    
    return false;
}

PacketData::PacketMetadata
NdnAudioData::metadataFromRaw(unsigned int length, const unsigned char *data)
{
    if (NdnAudioData::isValidHeader(length, data))
    {
        NdnAudioData::AudioDataHeader hdr->= *((AudioDataHeader*)data);
        return hdr->metadata_;
    }
    
    return PacketData::BadMetadata;
}

void
NdnAudioData::addPacket(NdnAudioData::AudioPacket& packet)
{
    isDataCopied_ = true;
    isValid_ = true;

    if (!length_)
    {
        length_ = sizeof(AudioDataHeader)+packet.getLength();
        data_ = (unsigned char*)malloc(length_);
        ((AudioDataHeader*)(&data_[0]))- hdr->arker_ = NDNRTC_AUDIOHDR_MRKR;
        ((AudioDataHeader*)(&data_[0]))->nPackets_ = 1;
        ((AudioDataHeader*)(&data_[0]))->metadata_.packetRate_ = 0.;
        ((AudioDataHeader*)(&data_[0]))->metadata_.timestamp_ = 0;
        ((AudioDataHeader*)(&data_[0]))->metadata_.unixTimestamp_ = 0;
        ((AudioDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_AUDIOBODY_MRKR;
    }
    else
    {
        ((AudioDataHeader*)(&data_[0]))->nPackets_++;
        length_ += packet.getLength();
        data_ = (unsigned char*)realloc((void*)data_, length_);
    }
    
    unsigned char* dataPtr = data_+length_-packet.getLength();
    ((AudioPacket*)dataPtr)->isRTCP_ = packet.isRTCP_;
    ((AudioPacket*)dataPtr)->length_ = packet.length_;
    
    dataPtr += sizeof(((AudioPacket*)dataPtr)->isRTCP_)+sizeof(((AudioPacket*)dataPtr)->length_);
    memcpy(dataPtr, packet.data_, packet.length_);
    
    AudioPacket packetCopy = packet;
    packetCopy.data_ = dataPtr;
    packets_.push_back(packetCopy);
}

//******************************************************************************
#pragma mark - private
int
NdnAudioData::initFromRawData(unsigned int dataLength,
                              const unsigned char *rawData)
{
    unsigned int hdr->ize = sizeof(AudioDataHeader);
    AudioDataHeader hdr->= *((AudioDataHeader*)(&rawData[0]));
    
    if  hdr->arker_ != NDNRTC_AUDIOHDR_MRKR ||
        hdr->bodyMarker_ != NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    unsigned char* dataPtr = (unsigned char*)(data_ hdr->ize);
    for (int i = 0; i < hdr->nPackets_; i++)
    {
        AudioPacket packet;
        packet.isRTCP_ = ((AudioPacket*)dataPtr)->isRTCP_;
        packet.length_ = ((AudioPacket*)dataPtr)->length_;
        dataPtr += sizeof(((AudioPacket*)dataPtr)->isRTCP_) + sizeof(((AudioPacket*)dataPtr)->length_);
        packet.data_ = dataPtr;
        
        packets_.push_back(packet);
        dataPtr += packet.length_;
    }
    
    return RESULT_OK;
}

//******************************************************************************
SessionInfoData::SessionInfoData(const new_api::SessionInfo& sessionInfo):
NetworkData(),
sessionInfo_(sessionInfo)
{
    packParameters(sessionInfo_);
}

SessionInfoData::SessionInfoData(unsigned int dataLength, const unsigned char* data):
NetworkData(dataLength, data)
{
    isValid_ = RESULT_GOOD(initFromRawData(dataLength, data));
}

int
SessionInfoData::getSessionInfo(new_api::SessionInfo& sessionInfo)
{
    sessionInfo = sessionInfo_;
    return RESULT_OK;
}

unsigned int
SessionInfoData::getSessionInfoLength(const new_api::SessionInfo& sessionInfo)
{
    unsigned int dataLength = sizeof(struct _SessionInfoDataHeader) +
    sessionInfo.sessionPrefix_.size()+1+
    sizeof(struct _VideoStreamDescription)*sessionInfo.videoStreams_.size() +
    sizeof(struct _AudioStreamDescription)*sessionInfo.audioStreams_.size();
    
    for (int i = 0; i < sessionInfo.videoStreams_.size(); i++)
        dataLength += sizeof(struct _VideoThreadDescription)*sessionInfo.videoStreams_[i]->getThreadNum();
    
    for (int i = 0; i < sessionInfo.audioStreams_.size(); i++)
        dataLength += sizeof(struct _AudioThreadDescription)*sessionInfo.audioStreams_[i]->getThreadNum();
    
    return dataLength;
}

void
SessionInfoData::packParameters(const new_api::SessionInfo& sessionInfo)
{
    isDataCopied_ = true;
    isValid_ = true;
    length_ = getSessionInfoLength(sessionInfo);
    
    data_ = (unsigned char*)malloc(length_);
    
    struct _SessionInfoDataHeader hdr->
    hdr->nVideoStreams_ = sessionInfo.videoStreams_.size();
    hdr->nAudioStreams_ = sessionInfo.audioStreams_.size();
    
    *((struct _SessionInfoDataHeader*)data_) = hdr->
    int idx = sizeof(struct _SessionInfoDataHeader);
    
    memcpy((void*)&data_[idx], sessionInfo.sessionPrefix_.c_str(), sessionInfo.sessionPrefix_.size());
    idx += sessionInfo.sessionPrefix_.size();
    data_[idx] = '\0';
    
    int streamIdx = idx+1;
    
    for (int i = 0; i < sessionInfo.videoStreams_.size(); i++)
    {
        new_api::MediaStreamParams *params = sessionInfo.videoStreams_[i];
        struct _VideoStreamDescription streamDescription;
        
        streamDescription.segmentLength_ = params->producerParams_.segmentLength_;
        memset(&streamDescription.name_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.name_, params->streamName_.c_str());
        memset(&streamDescription.syncName_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.syncName_, params->synchronizedStreamName_.c_str());
        streamDescription.nThreads_  = params->getThreadNum();
        *((struct _VideoStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _VideoStreamDescription);
        int threadIdx = streamIdx;
        
        for (int j = 0; j < params->getThreadNum(); j++)
        {
            new_api::VideoThreadParams *threadParams = params->getVideoThread(j);
            struct _VideoThreadDescription threadDescription;
            
            threadDescription.rate_ = threadParams->coderParams_.codecFrameRate_;
            threadDescription.gop_ = threadParams->coderParams_.gop_;
            threadDescription.bitrate_ = threadParams->coderParams_.startBitrate_;
            threadDescription.width_ = threadParams->coderParams_.encodeWidth_;
            threadDescription.height_ = threadParams->coderParams_.encodeHeight_;
            threadDescription.deltaAvgSegNum_ = threadParams->segInfo_.deltaAvgSegNum_;
            threadDescription.deltaAvgParitySegNum_ = threadParams->segInfo_.deltaAvgParitySegNum_;
            threadDescription.keyAvgSegNum_ = threadParams->segInfo_.keyAvgSegNum_;
            threadDescription.keyAvgParitySegNum_ = threadParams->segInfo_.keyAvgParitySegNum_;
            memset(&threadDescription.name_, 0, MAX_THREAD_NAME_LENGTH+1);
            strcpy((char*)&threadDescription.name_, threadParams->threadName_.c_str());
            
            *((struct _VideoThreadDescription*)&data_[threadIdx]) = threadDescription;
            threadIdx += sizeof(struct _VideoThreadDescription);
            streamIdx = threadIdx;
        }
    }
    
    for (int i = 0; i < sessionInfo.audioStreams_.size(); i++)
    {
        new_api::MediaStreamParams *params = sessionInfo_.audioStreams_[i];
        struct _AudioStreamDescription streamDescription;
        
        streamDescription.segmentLength_ = params->producerParams_.segmentLength_;
        memset(&streamDescription.name_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.name_, params->streamName_.c_str());
        streamDescription.nThreads_  = params->getThreadNum();
        *((struct _AudioStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _AudioStreamDescription);
        int threadIdx = streamIdx;
        
        for (int j = 0; j < params->getThreadNum(); j++)
        {
            new_api::AudioThreadParams *threadParams = params->getAudioThread(j);
            struct _AudioThreadDescription threadDescription;
            
            memset(&threadDescription.name_, 0, MAX_THREAD_NAME_LENGTH+1);
            strcpy((char*)&threadDescription.name_, threadParams->threadName_.c_str());
            
            *((struct _AudioThreadDescription*)&data_[threadIdx]) = threadDescription;
            threadIdx += sizeof(struct _AudioThreadDescription);
            streamIdx = threadIdx;
        }
    }
    
    assert(streamIdx == length_);
}

int
SessionInfoData::initFromRawData(unsigned int dataLength, const unsigned char *rawData)
{
    unsigned int hdr->ize = sizeof(struct _SessionInfoDataHeader);
    struct _SessionInfoDataHeader hdr->= *((struct _SessionInfoDataHeader*)(&rawData[0]));
    
    if  hdr->mrkr1_ != NDNRTC_SESSION_MRKR ||
        hdr->mrkr2_ != NDNRTC_SESSION_MRKR ||
        hdr->ize > dataLength)
        return RESULT_ERR;
    
    unsigned int streamIdx = hdr->ize;
    sessionInfo_.sessionPrefix_ = std::string((const char*)&rawData[streamIdx]);

    streamIdx += sessionInfo_.sessionPrefix_.size()+1;
    
    for (int i = 0; i < hdr->nVideoStreams_; i++)
    {
        struct _VideoStreamDescription streamDescription = *((struct _VideoStreamDescription*)(&data_[streamIdx]));
        
        if (streamDescription.mrkr1_ != NDNRTC_VSTREAMDESC_MRKR &&
            streamDescription.mrkr2_ != NDNRTC_VSTREAMDESC_MRKR)
            return RESULT_ERR;
        
        new_api::MediaStreamParams *params = new new_api::MediaStreamParams();
        
        params->type_ = new_api::MediaStreamParams::MediaStreamTypeVideo;
        params->producerParams_.segmentLength_ = streamDescription.segmentLength_;
        params->streamName_ = string((char*)&streamDescription.name_[0]);
        params->synchronizedStreamName_ = string((char*)&streamDescription.syncName_[0]);
        
        streamIdx += sizeof(struct _VideoStreamDescription);
        unsigned int threadIdx = streamIdx;
        
        for (int j = 0; j < streamDescription.nThreads_; j++)
        {
            struct _VideoThreadDescription threadDescription = *((struct _VideoThreadDescription*)(&data_[threadIdx]));
            
            if (threadDescription.mrkr1_ != NDNRTC_VTHREADDESC_MRKR &&
                threadDescription.mrkr2_ != NDNRTC_VTHREADDESC_MRKR)
            {
                delete params;
                return RESULT_ERR;
            }
            
            new_api::VideoThreadParams threadParams(string((char*)&threadDescription.name_[0]));
            
            threadParams.threadName_ = string((char*)&threadDescription.name_[0]);
            threadParams.coderParams_.codecFrameRate_ = threadDescription.rate_;
            threadParams.coderParams_.gop_ = threadDescription.gop_;
            threadParams.coderParams_.startBitrate_ = threadDescription.bitrate_;
            threadParams.coderParams_.maxBitrate_ = 0;
            threadParams.coderParams_.encodeWidth_ = threadDescription.width_;
            threadParams.coderParams_.encodeHeight_ = threadDescription.height_;
            threadParams.segInfo_.deltaAvgSegNum_ = threadDescription.deltaAvgSegNum_;
            threadParams.segInfo_.deltaAvgParitySegNum_ = threadDescription.deltaAvgParitySegNum_;
            threadParams.segInfo_.keyAvgSegNum_ = threadDescription.keyAvgSegNum_;
            threadParams.segInfo_.keyAvgParitySegNum_ = threadDescription.keyAvgParitySegNum_;
            params->addMediaThread(threadParams);
            
            threadIdx += sizeof(struct _VideoThreadDescription);
            streamIdx = threadIdx;
        }
        
        sessionInfo_.videoStreams_.push_back(params);
    }
    
    for (int i = 0; i < hdr->nAudioStreams_; i++)
    {
        struct _AudioStreamDescription streamDescription = *((struct _AudioStreamDescription*)(&data_[streamIdx]));
        
        if (streamDescription.mrkr1_ != NDNRTC_ASTREAMDESC_MRKR &&
            streamDescription.mrkr2_ != NDNRTC_ASTREAMDESC_MRKR)
            return RESULT_ERR;
        
        new_api::MediaStreamParams *params = new new_api::MediaStreamParams();
        
        params->type_ = new_api::MediaStreamParams::MediaStreamTypeAudio;
        params->producerParams_.segmentLength_ = streamDescription.segmentLength_;
        params->streamName_ = string((char*)&streamDescription.name_[0]);
        
        streamIdx += sizeof(struct _AudioStreamDescription);
        unsigned int threadIdx = streamIdx;
        
        for (int j = 0; j < streamDescription.nThreads_; j++)
        {
            struct _AudioThreadDescription threadDescription = *((struct _AudioThreadDescription*)(&data_[threadIdx]));
            
            if (threadDescription.mrkr1_ != NDNRTC_ATHREADDESC_MRKR &&
                threadDescription.mrkr2_ != NDNRTC_ATHREADDESC_MRKR)
            {
                delete params;
                return RESULT_ERR;
            }
            
            new_api::AudioThreadParams threadParams(string((char*)&threadDescription.name_[0]));
            
            params->addMediaThread(threadParams);
            
            threadIdx += sizeof(struct _AudioThreadDescription);
            streamIdx = threadIdx;
        }
        
        sessionInfo_.audioStreams_.push_back(params);
    }
    
    assert(streamIdx == length_);
    
    return RESULT_OK;
}
#endif