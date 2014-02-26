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

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnFrameData::NdnFrameData(EncodedImage &frame)
{
    unsigned int headerSize = sizeof(FrameDataHeader);
    
    length_ = frame._length+headerSize;
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
}

NdnFrameData::NdnFrameData(EncodedImage &frame, PacketMetadata &metadata):
NdnFrameData(frame)
{
    ((FrameDataHeader*)(&data_[0]))->metadata_ = metadata;
}

NdnFrameData::~NdnFrameData()
{
}

//******************************************************************************
#pragma mark - public
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
#pragma mark - construction/destruction
NdnAudioData::NdnAudioData(AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    
    length_ = packet.length_+headerSize;
    data_ = (unsigned char*)malloc(length_);
    
    memcpy(data_+headerSize, packet.data_, packet.length_);
    
    ((AudioDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
    ((AudioDataHeader*)(&data_[0]))->isRTCP_ = packet.isRTCP_;
    ((AudioDataHeader*)(&data_[0]))->timestamp_ = packet.timestamp_;
    ((AudioDataHeader*)(&data_[0]))->metadata_ = {0., 0};
    ((AudioDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_AUDIOBODY_MRKR;
}

NdnAudioData::NdnAudioData(AudioPacket &packet, PacketMetadata &metadata):
NdnAudioData(packet)
{
    ((AudioDataHeader*)(&data_[0]))->metadata_ = metadata;
}

//******************************************************************************
#pragma mark - public
int NdnAudioData::unpackAudio(unsigned int len, const unsigned char *data,
                              AudioPacket &packet)
{
    unsigned int headerSize = sizeof(AudioDataHeader);
    AudioDataHeader header = *((AudioDataHeader*)(&data[0]));
    
    if (header.headerMarker_ != NDNRTC_AUDIOHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_AUDIOBODY_MRKR)
        return RESULT_ERR;
    
    packet.isRTCP_ = header.isRTCP_;
    packet.timestamp_ = header.timestamp_;
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
    
    return header.timestamp_;
}
