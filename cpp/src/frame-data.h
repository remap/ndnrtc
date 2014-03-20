//
//  frame-data.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__frame_slot__
#define __ndnrtc__frame_slot__

#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d
#define NDNRTC_AUDIOHDR_MRKR 0xa4a4
#define NDNRTC_AUDIOBODY_MRKR 0xabad
#define NDNRTC_SEGHDR_MRKR 0xb4b4
#define NDNRTC_SEGBODY_MRKR 0xb5b5

namespace ndnrtc {
    
    /**
     * This stucture is used for storing meta info carried by data name prefixes
     */
    typedef struct _PrefixMetaInfo {
        int totalSegmentsNum_;
        PacketNumber playbackNo_;
        PacketNumber pairedSequenceNo_;
        
        static Name toName(const _PrefixMetaInfo &meta);
        static int extractMetadata(const Name& metaComponents, _PrefixMetaInfo &meta);
        
    } __attribute__((packed)) PrefixMetaInfo;
    
    class NetworkData
    {
    public:
        NetworkData() {}
        NetworkData(unsigned int dataLength, const unsigned char* rawData)
        {
            length_ = dataLength;
            data_ = (unsigned char*)malloc(dataLength);
            memcpy((void*)data_, (void*)rawData, length_);
            isDataCopied_ = true;
        }
        virtual ~NetworkData(){
            if (data_ && isDataCopied_)
                free(data_);
        }
        
        int
        getLength() const { return length_; }
        
        unsigned char*
        getData() const { return data_; }
        
        bool
        isValid() const { return isValid_; }
        
    protected:
        bool isValid_ = false, isDataCopied_ = false;
        unsigned int length_;
        unsigned char *data_ = NULL;
        
        virtual int
        initFromRawData(unsigned int dataLength,
                        const unsigned char* rawData) = 0;
    };
    
    class SegmentData : public NetworkData
    {
    public:
        typedef struct _SegmentMetaInfo {
            int32_t interestNonce_;
            int64_t interestArrivalMs_;
            int32_t generationDelayMs_;
        } __attribute__((packed)) SegmentMetaInfo;
        
        SegmentData(){}
        SegmentData(const unsigned char *segmentData, const unsigned int dataSize,
                    SegmentMetaInfo metadata = {0,0,0});
        
        SegmentMetaInfo
        getMetadata() { return ((SegmentHeader*)(&data_[0]))->metaInfo_; }
        
        unsigned char*
        getSegmentData() { return data_+SegmentData::getHeaderSize(); }
        
        unsigned int
        getSegmentDataLength() { return length_-SegmentData::getHeaderSize(); }
        
        inline static unsigned int
        getHeaderSize() { return sizeof(SegmentHeader); }
        
        int
        initFromRawData(unsigned int dataLength,
                        const unsigned char* rawData);
        
        static int
        segmentDataFromRaw(unsigned int dataLength,
                           const unsigned char* rawData,
                           SegmentData &segmentData);
        
    private:
        typedef struct _SegmentHeader {
            uint16_t headerMarker_ = NDNRTC_SEGHDR_MRKR;
            SegmentMetaInfo metaInfo_;
            uint16_t bodyMarker_ = NDNRTC_SEGBODY_MRKR;
        }  __attribute__((packed)) SegmentHeader;
        
    };

    
    /**
     * Base class for storing media data for publishing in ndn
     */
    class PacketData : public NetworkData
    {
    public:
        enum PacketDataType {
            TypeVideo = 1,
            TypeAudio = 2
        };
        struct PacketMetadata {
            double packetRate_; // current packet production rate
            int64_t timestamp_; // packet timestamp set by producer
        } __attribute__((packed));
        
        PacketData(){}
        PacketData(unsigned int dataLength, const unsigned char* rawData):
            NetworkData(dataLength, rawData) {}
        
        virtual PacketMetadata
        getMetadata();
        
        virtual PacketDataType
        getType() = 0;
        
        static int
        copyPacketFromRaw(unsigned int length, const unsigned char* data,
                         PacketData **packetData);
        
        static int
        packetFromRaw(unsigned int length, unsigned char* data,
                      PacketData **packetData);
        
    };
    
    /**
     * Class is used for packaging encoded frame metadata and actual data for
     * transferring over the network.
     * It has also methods for unarchiving this data into an encoded frame.
     */
    class NdnFrameData : public PacketData
    {
    public:
        NdnFrameData(unsigned int length, const unsigned char* rawData);
        NdnFrameData(const webrtc::EncodedImage &frame);
        NdnFrameData(const webrtc::EncodedImage &frame, PacketMetadata &metadata);
        NdnFrameData(){}
        ~NdnFrameData();
        
        PacketDataType
        getType() { return TypeVideo; }
        
        int
        getFrame(webrtc::EncodedImage& frame);
        
        PacketMetadata
        getMetadata();

        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        //******************************************************************************
        // old api
        static int
        unpackFrame(unsigned int length_, const unsigned char *data,
                    webrtc::EncodedImage **frame) DEPRECATED;
        
        static int
        unpackMetadata(unsigned int length_,
                       const unsigned char *data,
                       PacketMetadata &metadata) DEPRECATED;
        
        static
        webrtc::VideoFrameType getFrameTypeFromHeader(unsigned int size,
                                                      const unsigned char *headerSegment) DEPRECATED;
        
        static bool
        isVideoData(unsigned int size,
                    const unsigned char *headerSegment) DEPRECATED;
        
        static int64_t
        getTimestamp(unsigned int size,
                     const unsigned char *headerSegment) DEPRECATED;
        //******************************************************************************
        
    protected:
        struct FrameDataHeader {
            uint16_t                    headerMarker_ = NDNRTC_FRAMEHDR_MRKR;   // 2
            uint32_t                    encodedWidth_;                          // 6
            uint32_t                    encodedHeight_;                         // 10
            uint32_t                    timeStamp_;                             // 14
            int64_t                     capture_time_ms_;                       // 22
            webrtc::VideoFrameType      frameType_;                             // 26
                                                                                //            uint8_t*                    _buffer;
                                                                                //            uint32_t                    _length;
                                                                                //            uint32_t                    _size;
            bool                        completeFrame_;                         // 27
            PacketMetadata               metadata_;                             // 39
            uint16_t                    bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;    // 41
        } __attribute__((packed));
        
        webrtc::EncodedImage frame_;
        
        FrameDataHeader
        getHeader()
        {
            return *((FrameDataHeader*)(&data_[0]));
        }
    };
    
    /**
     * Class is used for packaging audio samples actual data and metadata for
     * transferring over the network.
     */
    class NdnAudioData : public PacketData
    {
    public:
        typedef struct _AudioPacket {
            bool isRTCP_;
            int64_t timestamp_;
            unsigned int length_;
            unsigned char *data_;
        } AudioPacket;
        
        NdnAudioData(unsigned int dataLength, const unsigned char* rawData);
        NdnAudioData(AudioPacket &packet);
        NdnAudioData(AudioPacket &packet, PacketMetadata &metadata);
        NdnAudioData(){}
        ~NdnAudioData(){}
        
        PacketDataType
        getType() { return TypeAudio; }
        
        int
        getAudioPacket(AudioPacket &audioPacket);
        
        PacketMetadata
        getMetadata();
        
        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        //******************************************************************************
        // old api
        static int
        unpackAudio(unsigned int len, const unsigned char *data,
                    AudioPacket &packet) DEPRECATED;
        
        static int
        unpackMetadata(unsigned int len, const unsigned char *data,
                       PacketMetadata &metadata) DEPRECATED;
        static bool
        isAudioData(unsigned int size,
                    const unsigned char *headerSegment) DEPRECATED;
        
        static int64_t
        getTimestamp(unsigned int size,
                     const unsigned char *headerSegment) DEPRECATED;
        //******************************************************************************
    private:
        struct AudioDataHeader {
            uint16_t        headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
            bool                isRTCP_;
            PacketMetadata      metadata_;
            uint16_t        bodyMarker_  = NDNRTC_AUDIOBODY_MRKR;
        };
        
        AudioPacket packet_;
        
        AudioDataHeader
        getHeader()
        {
            return *((AudioDataHeader*)(&data_[0]));
        }
    };
    
};

#endif /* defined(__ndnrtc__frame_slot__) */
