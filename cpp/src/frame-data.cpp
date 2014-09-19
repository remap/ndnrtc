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
#include "params.h"
#include "fec.h"

#define PREFIX_META_NCOMP 4

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
    
    if (header.encodedHeight_ > MaxHeight ||
        header.encodedWidth_ > MaxWidth ||
        header.encodedHeight_ < MinHeight ||
        header.encodedWidth_ < MinWidth)
        // got suspicious data, return error
        return RESULT_ERR;
    
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

NdnAudioData::NdnAudioData(AudioPacket &packet)
{
    initialize(packet);
}

NdnAudioData::NdnAudioData(AudioPacket &packet, PacketMetadata &metadata)
{
    initialize(packet);
    ((AudioDataHeader*)(&data_[0]))->metadata_ = metadata;
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

int
NdnAudioData::getAudioPacket(AudioPacket &audioPacket)
{
    if (!isValid())
        return RESULT_ERR;
    
    audioPacket = packet_;
    
    return RESULT_OK;
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

//******************************************************************************
#pragma mark - private
void
NdnAudioData::initialize(AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    
    length_ = packet.length_+headerSize;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(length_);
    
    memcpy(data_+headerSize, packet.data_, packet.length_);
    
    ((AudioDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
    ((AudioDataHeader*)(&data_[0]))->isRTCP_ = packet.isRTCP_;
    ((AudioDataHeader*)(&data_[0]))->metadata_.packetRate_ = 0.;
    ((AudioDataHeader*)(&data_[0]))->metadata_.timestamp_ = 0;
    ((AudioDataHeader*)(&data_[0]))->metadata_.unixTimestamp_ = 0;
    ((AudioDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_AUDIOBODY_MRKR;
    
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}

int
NdnAudioData::initFromRawData(unsigned int dataLength,
                              const unsigned char *rawData)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&rawData[0]));
    
    if (header.headerMarker_ != NDNRTC_AUDIOHDR_MRKR ||
        header.bodyMarker_ != NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    packet_.isRTCP_ = header.isRTCP_;
    packet_.length_ = dataLength-headerSize;
    packet_.data_ = (unsigned char*)rawData+headerSize;
    
    return RESULT_OK;
}

//******************************************************************************
SessionInfoData::SessionInfoData(const new_api::SessionInfo& sessionInfo):
NetworkData()
{
//    packParameters(videoParams, audioParams);
}

SessionInfoData::SessionInfoData(unsigned int dataLength, const unsigned char* data):
NetworkData(dataLength, data)
//videoParams_(DefaultParams),
//audioParams_(DefaultParamsAudio)
{
    isValid_ = RESULT_GOOD(initFromRawData(dataLength, data));
}

int
SessionInfoData::getParams(ParamsStruct& videoParams, ParamsStruct& audioParams) const
{
    return RESULT_ERR;
}

unsigned int
SessionInfoData::getSessionInfoLength(unsigned int nVideoStreams,
                                  unsigned int nAudioStreams)
{
    return sizeof(struct _SessionInfoDataHeader) +
    sizeof(struct _VideoStreamDescription) * nVideoStreams +
    sizeof(struct _AudioStreamDescription) * nAudioStreams;
}

void
SessionInfoData::packParameters(const ParamsStruct& videoParams,
                            const ParamsStruct& audioParams)
{
    isDataCopied_ = true;
    length_ = getSessionInfoLength(videoParams.nStreams, audioParams.nStreams);
    
    data_ = (unsigned char*)malloc(length_);
    
    struct _SessionInfoDataHeader header;
    header.nVideoStreams_ = videoParams.nStreams;
    header.videoSegmentSize_ = videoParams.segmentSize;
    header.nAudioStreams_ = audioParams.nStreams;
    header.audioSegmentSize_ = audioParams.segmentSize;
    
    *((struct _SessionInfoDataHeader*)data_) = header;
    
    int streamIdx = sizeof(struct _SessionInfoDataHeader);
    
    for (int i = 0; i < videoParams.nStreams; i++) {
        struct _VideoStreamDescription streamDescription;
        streamDescription.rate_ = videoParams.streamsParams[i].codecFrameRate;
        streamDescription.gop_ = videoParams.streamsParams[i].gop;
        streamDescription.bitrate_ = videoParams.streamsParams[i].startBitrate;
        streamDescription.width_ = videoParams.streamsParams[i].encodeWidth;
        streamDescription.height_ = videoParams.streamsParams[i].encodeHeight;
        
        *((struct _VideoStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _VideoStreamDescription);
    }
    
    for (int i = 0; i < audioParams.nStreams; i++){
        struct _AudioStreamDescription streamDescription;
        streamDescription.rate_ = audioParams.streamsParams[i].codecFrameRate;
        streamDescription.bitrate_ = audioParams.streamsParams[i].startBitrate;
        
        *((struct _AudioStreamDescription*)&data_[streamIdx]) = streamDescription;
        
        streamIdx += sizeof(struct _AudioStreamDescription);
    }
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
    
    videoParams_.segmentSize = header.videoSegmentSize_;
    audioParams_.segmentSize = header.audioSegmentSize_;
    
    if (dataLength < getSessionInfoLength(header.nVideoStreams_, header.nAudioStreams_))
        return RESULT_ERR;
    
    unsigned int streamIdx = headerSize;
    
    for (int i = 0; i < header.nVideoStreams_; i++)
    {
        struct _VideoStreamDescription streamDescription = *((struct _VideoStreamDescription*)(&data_[streamIdx]));
        CodecParams codecParams;
        memset(&codecParams, 0, sizeof(CodecParams));
        
        codecParams.idx = i;
        codecParams.codecFrameRate = streamDescription.rate_;
        codecParams.gop = streamDescription.gop_;
        codecParams.startBitrate = streamDescription.bitrate_;
        codecParams.encodeWidth = streamDescription.width_;
        codecParams.encodeHeight = streamDescription.height_;
        
        videoParams_.addNewStream(codecParams);
        streamIdx += sizeof(struct _VideoStreamDescription);
    }
    
    for (int i = 0; i < header.nAudioStreams_; i++)
    {
        struct _AudioStreamDescription streamDescription = *((struct _AudioStreamDescription*)(&data_[streamIdx]));
        CodecParams codecParams;
        memset(&codecParams, 0, sizeof(CodecParams));
        
        codecParams.idx = i;
        codecParams.codecFrameRate = streamDescription.rate_;
        codecParams.startBitrate = streamDescription.bitrate_;
        
        audioParams_.addNewStream(codecParams);
        streamIdx += sizeof(struct _AudioStreamDescription);
    }
    
    return RESULT_OK;
}

void
SessionInfoData::updateParams(ParamsStruct& paramsForUpdate,
                          const ParamsStruct& params)
{
    if (paramsForUpdate.nStreams)
    {
        free(paramsForUpdate.streamsParams);
        paramsForUpdate.streamsParams = NULL;
    }
    
    paramsForUpdate.segmentSize = params.segmentSize;
    paramsForUpdate.nStreams = 0;
    for (int i = 0; i < params.nStreams; i++)
        paramsForUpdate.addNewStream(params.streamsParams[i]);
}
