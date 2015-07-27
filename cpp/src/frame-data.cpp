//
//  frame-data.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "frame-data.h"
#include "ndnrtc-utils.h"
#include "params.h"
#include "fec.h"

#define PREFIX_META_NCOMP 5

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace fec;

//******************************************************************************
Name
PrefixMetaInfo::toName(const PrefixMetaInfo &meta)
{
    Name metaSuffix("");
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.totalSegmentsNum_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.playbackNo_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.pairedSequenceNo_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.paritySegmentsNum_));
    metaSuffix.append(NdnRtcUtils::componentFromInt(meta.crcValue_));
    
    return metaSuffix;
}

int
PrefixMetaInfo::extractMetadata(const ndn::Name &prefix,
                                PrefixMetaInfo &meta)
{
    if (prefix.size() >= PREFIX_META_NCOMP)
    {
        meta.totalSegmentsNum_ = NdnRtcUtils::intFromComponent(prefix[0-PREFIX_META_NCOMP]);
        meta.playbackNo_ = NdnRtcUtils::intFromComponent(prefix[1-PREFIX_META_NCOMP]);
        meta.pairedSequenceNo_ = NdnRtcUtils::intFromComponent(prefix[2-PREFIX_META_NCOMP]);
        meta.paritySegmentsNum_ = NdnRtcUtils::intFromComponent(prefix[3-PREFIX_META_NCOMP]);
        meta.crcValue_ = NdnRtcUtils::intFromComponent(prefix[4-PREFIX_META_NCOMP]);
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

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
    unsigned int headerSize = sizeof(SegmentHeader);
    
    length_ = headerSize + dataSize;
    
    isValid_ = true;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(length_);
    memcpy(data_+headerSize, segmentData, dataSize);
    
    ((SegmentHeader*)(&data_[0]))->headerMarker_ = NDNRTC_SEGHDR_MRKR;
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
        ((SegmentHeader*)(&rawData[0]))->headerMarker_ == NDNRTC_SEGHDR_MRKR &&
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
                                    unsigned int segmentSize)
{
    uint32_t nParitySegments = getParitySegmentsNum(nSegments, parityRatio);
    
    length_ = getParityDataLength(nSegments, parityRatio, segmentSize);
    data_ = (unsigned char*)malloc(length_);
    isDataCopied_ = true;
    
    // create redundancy data
    Rs28Encoder enc(nSegments, nParitySegments, segmentSize);
    
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
(unsigned int nSegments, double parityRatio, unsigned int segmentSize)
{
    return getParitySegmentsNum(nSegments, parityRatio) * segmentSize;
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnFrameData::NdnFrameData(unsigned int length, const unsigned char* rawData):
PacketData(length, rawData)
{
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}
NdnFrameData::NdnFrameData(const EncodedImage &frame,
                           unsigned int segmentSize)
{
    initialize(frame, segmentSize);
}

NdnFrameData::NdnFrameData(const EncodedImage &frame, unsigned int segmentSize,
                           PacketMetadata &metadata)
{
    initialize(frame, segmentSize);
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
    unsigned int headerSize = sizeof(FrameDataHeader);
    
    if (length >= headerSize)
    {
        FrameDataHeader header = *((FrameDataHeader*)(&data[0]));

        if (header.headerMarker_ == NDNRTC_FRAMEHDR_MRKR &&
            header.bodyMarker_ == NDNRTC_FRAMEBODY_MRKR)
            return true;
    }
    
    return false;
}

PacketData::PacketMetadata
NdnFrameData::metadataFromRaw(unsigned int length, const unsigned char *data)
{
    if (NdnFrameData::isValidHeader(length, data))
    {
        NdnFrameData::FrameDataHeader header = *((FrameDataHeader*)data);
        return header.metadata_;
    }
    
    return PacketData::BadMetadata;
}

//******************************************************************************
#pragma mark - private
void
NdnFrameData::initialize(const webrtc::EncodedImage &frame, unsigned int segmentSize)
{
    unsigned int headerSize = sizeof(FrameDataHeader);
    unsigned int allocSize = (unsigned int)ceil((double)(frame._length+headerSize)/(double)segmentSize)*segmentSize;
    
    length_ = frame._length+headerSize;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(allocSize);
    memset(data_, 0, allocSize);
    
    // copy frame data with offset of header
    memcpy(data_+headerSize, frame._buffer, frame._length);
    
    // setup header
    ((FrameDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
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
    unsigned int headerSize = sizeof(FrameDataHeader);
    FrameDataHeader header = *((FrameDataHeader*)(&rawData[0]));
    
    // check markers
    if ((header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR ||
         header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR) ||
        dataLength < headerSize)
        return RESULT_ERR;
    
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, header.encodedWidth_,
                                          header.encodedHeight_);
        
    frame_ = webrtc::EncodedImage(const_cast<uint8_t*>(&rawData[headerSize]),
                                  dataLength-headerSize, size);
    frame_._encodedWidth = header.encodedWidth_;
    frame_._encodedHeight = header.encodedHeight_;
    frame_._timeStamp = header.timeStamp_;
    frame_.capture_time_ms_ = header.capture_time_ms_;
    frame_._frameType = header.frameType_;
    frame_._completeFrame = header.completeFrame_;

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
    unsigned int headerSize = sizeof(AudioDataHeader);
    
    if (length >= headerSize)
    {
        AudioDataHeader header = *((AudioDataHeader*)(&data[0]));
        
        if (header.headerMarker_ == NDNRTC_AUDIOHDR_MRKR &&
            header.bodyMarker_ == NDNRTC_AUDIOBODY_MRKR)
            return true;
    }
    
    return false;
}

PacketData::PacketMetadata
NdnAudioData::metadataFromRaw(unsigned int length, const unsigned char *data)
{
    if (NdnAudioData::isValidHeader(length, data))
    {
        NdnAudioData::AudioDataHeader header = *((AudioDataHeader*)data);
        return header.metadata_;
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
        ((AudioDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
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
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&rawData[0]));
    
    if (header.headerMarker_ != NDNRTC_AUDIOHDR_MRKR ||
        header.bodyMarker_ != NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    unsigned char* dataPtr = (unsigned char*)(data_+headerSize);
    for (int i = 0; i < header.nPackets_; i++)
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
        dataLength += sizeof(struct _VideoThreadDescription)*sessionInfo.videoStreams_[i]->mediaThreads_.size();
    
    for (int i = 0; i < sessionInfo.audioStreams_.size(); i++)
        dataLength += sizeof(struct _AudioThreadDescription)*sessionInfo.audioStreams_[i]->mediaThreads_.size();
    
    return dataLength;
}

void
SessionInfoData::packParameters(const new_api::SessionInfo& sessionInfo)
{
    isDataCopied_ = true;
    isValid_ = true;
    length_ = getSessionInfoLength(sessionInfo);
    
    data_ = (unsigned char*)malloc(length_);
    
    struct _SessionInfoDataHeader header;
    header.nVideoStreams_ = sessionInfo.videoStreams_.size();
    header.nAudioStreams_ = sessionInfo.audioStreams_.size();
    
    *((struct _SessionInfoDataHeader*)data_) = header;
    int idx = sizeof(struct _SessionInfoDataHeader);
    
    memcpy((void*)&data_[idx], sessionInfo.sessionPrefix_.c_str(), sessionInfo.sessionPrefix_.size());
    idx += sessionInfo.sessionPrefix_.size();
    data_[idx] = '\0';
    
    int streamIdx = idx+1;
    
    for (int i = 0; i < sessionInfo.videoStreams_.size(); i++)
    {
        new_api::MediaStreamParams *params = sessionInfo.videoStreams_[i];
        struct _VideoStreamDescription streamDescription;
        
        streamDescription.segmentSize_ = params->producerParams_.segmentSize_;
        memset(&streamDescription.name_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.name_, params->streamName_.c_str());
        memset(&streamDescription.syncName_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.syncName_, params->synchronizedStreamName_.c_str());
        streamDescription.nThreads_  = params->mediaThreads_.size();
        *((struct _VideoStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _VideoStreamDescription);
        int threadIdx = streamIdx;
        
        for (int j = 0; j < params->mediaThreads_.size(); j++)
        {
            new_api::VideoThreadParams *threadParams = (new_api::VideoThreadParams*)params->mediaThreads_[j];
            struct _VideoThreadDescription threadDescription;
            
#warning should this reflect actual encoding rate of a producer?
            threadDescription.rate_ = threadParams->coderParams_.codecFrameRate_;
            threadDescription.gop_ = threadParams->coderParams_.gop_;
            threadDescription.bitrate_ = threadParams->coderParams_.startBitrate_;
            threadDescription.width_ = threadParams->coderParams_.encodeWidth_;
            threadDescription.height_ = threadParams->coderParams_.encodeHeight_;
            threadDescription.deltaAvgSegNum_ = threadParams->deltaAvgSegNum_;
            threadDescription.deltaAvgParitySegNum_ = threadParams->deltaAvgParitySegNum_;
            threadDescription.keyAvgSegNum_ = threadParams->keyAvgSegNum_;
            threadDescription.keyAvgParitySegNum_ = threadParams->keyAvgParitySegNum_;
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
        
        streamDescription.segmentSize_ = params->producerParams_.segmentSize_;
        memset(&streamDescription.name_, 0, MAX_STREAM_NAME_LENGTH+1);
        strcpy((char*)&streamDescription.name_, params->streamName_.c_str());
        streamDescription.nThreads_  = params->mediaThreads_.size();
        *((struct _AudioStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _AudioStreamDescription);
        int threadIdx = streamIdx;
        
        for (int j = 0; j < params->mediaThreads_.size(); j++)
        {
            new_api::AudioThreadParams *threadParams = (new_api::AudioThreadParams*)params->mediaThreads_[j];
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
    unsigned int headerSize = sizeof(struct _SessionInfoDataHeader);
    struct _SessionInfoDataHeader header = *((struct _SessionInfoDataHeader*)(&rawData[0]));
    
    if (header.mrkr1_ != NDNRTC_SESSION_MRKR ||
        header.mrkr2_ != NDNRTC_SESSION_MRKR ||
        headerSize > dataLength)
        return RESULT_ERR;
    
    unsigned int streamIdx = headerSize;
    sessionInfo_.sessionPrefix_ = std::string((const char*)&rawData[streamIdx]);

    streamIdx += sessionInfo_.sessionPrefix_.size()+1;
    
    for (int i = 0; i < header.nVideoStreams_; i++)
    {
        struct _VideoStreamDescription streamDescription = *((struct _VideoStreamDescription*)(&data_[streamIdx]));
        
        if (streamDescription.mrkr1_ != NDNRTC_VSTREAMDESC_MRKR &&
            streamDescription.mrkr2_ != NDNRTC_VSTREAMDESC_MRKR)
            return RESULT_ERR;
        
        new_api::MediaStreamParams *params = new new_api::MediaStreamParams();
        
        params->type_ = new_api::MediaStreamParams::MediaStreamTypeVideo;
        params->producerParams_.segmentSize_ = streamDescription.segmentSize_;
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
            
            new_api::VideoThreadParams *threadParams = new new_api::VideoThreadParams();
            
            threadParams->threadName_ = string((char*)&threadDescription.name_[0]);
            threadParams->coderParams_.codecFrameRate_ = threadDescription.rate_;
            threadParams->coderParams_.gop_ = threadDescription.gop_;
            threadParams->coderParams_.startBitrate_ = threadDescription.bitrate_;
            threadParams->coderParams_.maxBitrate_ = 0;
            threadParams->coderParams_.encodeWidth_ = threadDescription.width_;
            threadParams->coderParams_.encodeHeight_ = threadDescription.height_;
            threadParams->deltaAvgSegNum_ = threadDescription.deltaAvgSegNum_;
            threadParams->deltaAvgParitySegNum_ = threadDescription.deltaAvgParitySegNum_;
            threadParams->keyAvgSegNum_ = threadDescription.keyAvgSegNum_;
            threadParams->keyAvgParitySegNum_ = threadDescription.keyAvgParitySegNum_;
            params->mediaThreads_.push_back(threadParams);
            
            threadIdx += sizeof(struct _VideoThreadDescription);
            streamIdx = threadIdx;
        }
        
        sessionInfo_.videoStreams_.push_back(params);
    }
    
    for (int i = 0; i < header.nAudioStreams_; i++)
    {
        struct _AudioStreamDescription streamDescription = *((struct _AudioStreamDescription*)(&data_[streamIdx]));
        
        if (streamDescription.mrkr1_ != NDNRTC_ASTREAMDESC_MRKR &&
            streamDescription.mrkr2_ != NDNRTC_ASTREAMDESC_MRKR)
            return RESULT_ERR;
        
        new_api::MediaStreamParams *params = new new_api::MediaStreamParams();
        
        params->type_ = new_api::MediaStreamParams::MediaStreamTypeAudio;
        params->producerParams_.segmentSize_ = streamDescription.segmentSize_;
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
            
            new_api::AudioThreadParams *threadParams = new new_api::AudioThreadParams();
            
            threadParams->threadName_ = string((char*)&threadDescription.name_[0]);
            params->mediaThreads_.push_back(threadParams);
            
            threadIdx += sizeof(struct _AudioThreadDescription);
            streamIdx = threadIdx;
        }
        
        sessionInfo_.audioStreams_.push_back(params);
    }
    
    assert(streamIdx == length_);
    
    return RESULT_OK;
}
