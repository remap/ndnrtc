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
        int paritySegmentsNum_;
        
        static ndn::Name toName(const _PrefixMetaInfo &meta);
        static int extractMetadata(const ndn::Name& metaComponents, _PrefixMetaInfo &meta);
        
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
            TypeAudio = 2,
            TypeParity = 3
        };
        struct PacketMetadata {
            double packetRate_; // current packet production rate
            int64_t timestamp_; // packet timestamp set by producer
            double unixTimestamp_; // unix timestamp set by producer
        } __attribute__((packed));
        
        static const PacketMetadata ZeroMetadata;
        static const PacketMetadata BadMetadata;
        
        PacketData(){}
        PacketData(unsigned int dataLength, const unsigned char* rawData):
            NetworkData(dataLength, rawData) {}
        
        virtual PacketMetadata
        getMetadata();
        
        virtual void
        setMetadata(PacketMetadata& metadata) = 0;
        
        virtual PacketDataType
        getType() = 0;
        
        static int
        packetFromRaw(unsigned int length, unsigned char* data,
                      PacketData **packetData);
        
        /**
         * Retrieves metadata from raw bytes. It is assumed that these bytes 
         * represent 0 segment of the data (audio or video), i.e. control 
         * header markers are checked and if they are not satisfy video or audio 
         * packet, returned value is initialized with -1s.
         * @param lenght Length of data block
         * @param data Data block
         * @return packet metadata of type PacketMetadata
         */
        static PacketMetadata
        metadataFromRaw(unsigned int length, const unsigned char* data);
    };
    
    class FrameParityData: public PacketData
    {
    public:
        FrameParityData(unsigned int length, const unsigned char* rawData);
        FrameParityData(){}
        
        PacketDataType
        getType() { return TypeParity; }
        
        void
        setMetadata(PacketMetadata& metadata) {}

        int
        initFromPacketData(const PacketData& packetData,
                           double parityRatio,
                           unsigned int nSegments,
                           unsigned int segmentSize);

        
        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        static unsigned int
        getParitySegmentsNum(unsigned int nSegments, double parityRatio);
        
        static unsigned int
        getParityDataLength(unsigned int nSegments, double parityRatio,
                            unsigned int segmentSize);
    };
    
    /**
     * Class is used for packaging encoded frame metadata and actual data for
     * transferring over the network.
     * It has also methods for unarchiving this data into an encoded frame.
     * 
     */
    class NdnFrameData : public PacketData
    {
    public:
        NdnFrameData(unsigned int length, const unsigned char* rawData);
        NdnFrameData(const webrtc::EncodedImage &frame,
                     unsigned int segmentSize);
        NdnFrameData(const webrtc::EncodedImage &frame,
                     unsigned int segmentSize,
                     PacketMetadata &metadata);
        NdnFrameData(){}
        ~NdnFrameData();
        
        PacketDataType
        getType() { return TypeVideo; }
        
        int
        getFrame(webrtc::EncodedImage& frame);
        
        PacketMetadata
        getMetadata();
        
        void
        setMetadata(PacketMetadata& metadata);

        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        static bool
        isValidHeader(unsigned int length, const unsigned char* data);
        
        static PacketMetadata
        metadataFromRaw(unsigned int length, const unsigned char* data);
        
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
        unsigned int segmentSize_;
        
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
        
        void
        setMetadata(PacketMetadata& metadata);
        
        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        static bool
        isValidHeader(unsigned int length, const unsigned char* data);
        
        static PacketMetadata
        metadataFromRaw(unsigned int length, const unsigned char* data);
        
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
