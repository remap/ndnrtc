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

#include <ndn-fec/fec_encode.h>
#include <ndn-fec/fec_common.h>

#define PREFIX_META_NCOMP 4

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

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
    Name metaComponents(prefix);
    
    // get last four components
    if (metaComponents.size() > PREFIX_META_NCOMP)
        metaComponents = metaComponents.getSubName(prefix.size()-PREFIX_META_NCOMP, PREFIX_META_NCOMP);
    
    if (metaComponents.size() == PREFIX_META_NCOMP)
    {
        meta.totalSegmentsNum_ = NdnRtcUtils::intFromComponent(metaComponents[0]);
        meta.playbackNo_ = NdnRtcUtils::intFromComponent(metaComponents[1]);
        meta.pairedSequenceNo_ = NdnRtcUtils::intFromComponent(metaComponents[2]);
        meta.paritySegmentsNum_ = NdnRtcUtils::intFromComponent(metaComponents[3]);
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

//******************************************************************************
//******************************************************************************
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
ndnrtc::PacketData::copyPacketFromRaw(unsigned int length,
                                      const unsigned char *data,
                                      ndnrtc::PacketData **packetData)
{
    NdnFrameData *frameData = new NdnFrameData(length, data);
    
    if (frameData->isValid())
    {
        *packetData = frameData;
        return RESULT_OK;
    }
    else
    {
        NdnAudioData *audioData = new NdnAudioData(length, data);

        if (audioData->isValid())
        {
            *packetData = audioData;
            return RESULT_OK;
        }
        
        delete audioData;
    }
    
    delete frameData;
    
    return RESULT_ERR;
}
int
ndnrtc::PacketData::packetFromRaw(unsigned int length,
                                  unsigned char *data,
                                  ndnrtc::PacketData **packetData)
{
    NdnFrameData *frameData = new NdnFrameData();
    
    frameData->length_ = length;
    frameData->data_ = data;

    if (RESULT_GOOD(frameData->initFromRawData(frameData->length_, frameData->data_)))
    {
        frameData->isValid_ = true;
        *packetData = frameData;
        return RESULT_OK;
    }
    else
    {
        NdnAudioData *audioData = new NdnAudioData();

        audioData->length_ = length;
        audioData->data_ = data;
        
        if (RESULT_GOOD(audioData->initFromRawData(audioData->length_, audioData->data_)))
        {
            audioData->isValid_ = true;
            *packetData = audioData;
            return RESULT_OK;
        }
        
        delete audioData;
    }
    delete frameData;
    
    return RESULT_ERR;
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
FrameParityData::initFromFrame(const webrtc::EncodedImage& frame,
                               double parityRatio,
                               unsigned int nSegments,
                               unsigned int segmentSize)
{
    uint32_t dataLen = frame._length;
    uint32_t nParitySegments = getParitySegmentsNum(nSegments, parityRatio);
    
    length_ = getParityDataLength(nSegments, parityRatio, segmentSize);
    data_ = (unsigned char*)malloc(length_);
    isDataCopied_ = true;
    
    // create redundancy data
    Rs28Encode enc(nSegments+nParitySegments, nSegments, segmentSize);
    
    if (enc.encode((char*)frame._buffer, (char*)data_) < 0)
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
NdnFrameData::NdnFrameData(const EncodedImage &frame)
{
    unsigned int headerSize = sizeof(FrameDataHeader);

    length_ = frame._length+headerSize;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(length_);
    
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

NdnFrameData::NdnFrameData(const EncodedImage &frame, PacketMetadata &metadata):
NdnFrameData(frame)
{
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

//******************************************************************************
// old api
int NdnFrameData::unpackFrame(unsigned int length, const unsigned char *data,
                              webrtc::EncodedImage **frame)
{
    unsigned int headerSize = sizeof(FrameDataHeader);
    FrameDataHeader header = *((FrameDataHeader*)(&data[0]));
    
    // check markers
    if ((header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
         header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR) ||
        length < headerSize)
        return RESULT_ERR;
    
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, header.encodedWidth_,
                                          header.encodedHeight_);
    
    *frame = new webrtc::EncodedImage(const_cast<uint8_t*>(&data[headerSize]),
                                      length-headerSize, size);
    (*frame)->_encodedWidth = header.encodedWidth_;
    (*frame)->_encodedHeight = header.encodedHeight_;
    (*frame)->_timeStamp = header.timeStamp_;
    (*frame)->capture_time_ms_ = header.capture_time_ms_;
    (*frame)->_frameType = header.frameType_;
    (*frame)->_completeFrame = header.completeFrame_;
    
    return RESULT_OK;
}

int NdnFrameData::unpackMetadata(unsigned int length_,
                                 const unsigned char *data, ndnrtc::
                                 PacketData::PacketMetadata &metadata)
{
    if (!data)
        return RESULT_ERR;
    
    unsigned int headerSize = sizeof(FrameDataHeader);
    FrameDataHeader header = *((FrameDataHeader*)(&data[0]));
    
    // check markers
    if (header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR)
        return RESULT_ERR;
    
    metadata = header.metadata_;
    
    return RESULT_OK;
}

webrtc::VideoFrameType NdnFrameData::getFrameTypeFromHeader(unsigned int size,
                                                            const unsigned char *headerSegment)
{
    FrameDataHeader header = *((FrameDataHeader*)(&headerSegment[0]));
    
    // check markers if it's not video frame data - return key frame type always
    if (header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR)
        return webrtc::kDeltaFrame;
    
    return header.frameType_;
}

bool NdnFrameData::isVideoData(unsigned int size,
                               const unsigned char *headerSegment)
{
    FrameDataHeader header = *((FrameDataHeader*)(&headerSegment[0]));
    
    if (header.headerMarker_ == NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ == NDNRTC_FRAMEBODY_MRKR)
        return true;
    
    return false;
}

int64_t NdnFrameData::getTimestamp(unsigned int size,
                                   const unsigned char *headerSegment)
{
    if (!NdnFrameData::isVideoData(size, headerSegment))
        return -1;
    
    FrameDataHeader header = *((FrameDataHeader*)(&headerSegment[0]));
    
    return header.capture_time_ms_;
}
//******************************************************************************

//******************************************************************************
#pragma mark - private
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
    unsigned int headerSize = sizeof(AudioDataHeader);
    
    length_ = packet.length_+headerSize;
    isDataCopied_ = true;
    data_ = (unsigned char*)malloc(length_);
    
    memcpy(data_+headerSize, packet.data_, packet.length_);
    
    ((AudioDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
    ((AudioDataHeader*)(&data_[0]))->isRTCP_ = packet.isRTCP_;
    ((AudioDataHeader*)(&data_[0]))->metadata_ = {0., 0};
    ((AudioDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_AUDIOBODY_MRKR;
    
    isValid_ = RESULT_GOOD(initFromRawData(length_, data_));
}

NdnAudioData::NdnAudioData(AudioPacket &packet, PacketMetadata &metadata):
NdnAudioData(packet)
{
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

int
NdnAudioData::getAudioPacket(AudioPacket &audioPacket)
{
    if (!isValid())
        return RESULT_ERR;
    
    audioPacket = packet_;
    
    return RESULT_OK;
}

//******************************************************************************
// old api
int NdnAudioData::unpackAudio(unsigned int len, const unsigned char *data,
                              AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&data[0]));
    
    if (header.headerMarker_ != NDNRTC_AUDIOHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    packet.isRTCP_ = header.isRTCP_;
    packet.length_ = len-headerSize;
    packet.data_ = (unsigned char*)data+headerSize;
    
    return RESULT_OK;
}

int NdnAudioData::unpackMetadata(unsigned int len, const unsigned char *data,
                                 PacketData::PacketMetadata &metadata)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&data[0]));
    
    // check markers
    if (header.headerMarker_ != (uint16_t)NDNRTC_AUDIOHDR_MRKR &&
        header.bodyMarker_ != (uint16_t)NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    metadata = header.metadata_;
    
    return RESULT_OK;
    
}

bool NdnAudioData::isAudioData(unsigned int size,
                               const unsigned char *headerSegment)
{
    AudioDataHeader header = *((AudioDataHeader*)(&headerSegment[0]));
    
    if (header.headerMarker_ == (uint16_t)NDNRTC_AUDIOHDR_MRKR &&
        header.bodyMarker_ == (uint16_t)NDNRTC_AUDIOBODY_MRKR)
        return true;
    
    return false;
}

int64_t NdnAudioData::getTimestamp(unsigned int size,
                                   const unsigned char *headerSegment)
{
    if (!NdnAudioData::isAudioData(size, headerSegment))
        return -1;
    
    AudioDataHeader header = *((AudioDataHeader*)(&headerSegment[0]));
    
    return header.metadata_.timestamp_;
}
//******************************************************************************

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
    
    packet_.isRTCP_ = header.isRTCP_;
    packet_.length_ = dataLength-headerSize;
    packet_.data_ = (unsigned char*)rawData+headerSize;
    
    return RESULT_OK;
}