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

#include <boost/crc.hpp>

#include "ndnrtc-common.h"
#include "params.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d
#define NDNRTC_AUDIOHDR_MRKR 0xa4a4
#define NDNRTC_AUDIOBODY_MRKR 0xabad
#define NDNRTC_SEGHDR_MRKR 0xb4b4
#define NDNRTC_SEGBODY_MRKR 0xb5b5
#define NDNRTC_SESSION_MRKR 0x1234
#define NDNRTC_VSTREAMDESC_MRKR 0xb1b1
#define NDNRTC_ASTREAMDESC_MRKR 0xb2b2
#define NDNRTC_VTHREADDESC_MRKR 0xb3b3
#define NDNRTC_ATHREADDESC_MRKR 0xb4b4

namespace ndnrtc {
    
    /**
     * This stucture is used for storing meta info carried by data name prefixes
     */
    typedef struct _PrefixMetaInfo {
        int totalSegmentsNum_;
        PacketNumber playbackNo_;
        PacketNumber pairedSequenceNo_;
        int paritySegmentsNum_;
        int crcValue_;
        
        static ndn::Name toName(const _PrefixMetaInfo &meta);
        static int extractMetadata(const ndn::Name& metaComponents, _PrefixMetaInfo &meta);
        
    } __attribute__((packed)) PrefixMetaInfo;
    
    class NetworkData
    {
    public:
        NetworkData():length_(0), data_(NULL) {}
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
        
        int
        getCrcValue()
        {
            crc_computer_ = std::for_each(data_, data_+length_, crc_computer_);
            return crc_computer_();
        }
        
    protected:
        bool isValid_ = false, isDataCopied_ = false;
        unsigned int length_;
        unsigned char *data_ = NULL;
        boost::crc_16_type crc_computer_;
        
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
                    SegmentMetaInfo metadata = (SegmentMetaInfo){0,0,0});
        
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
            WebRtcVideoFrameType      frameType_;                             // 26
                                                                                //            uint8_t*                    _buffer;
                                                                                //            uint32_t                    _length;
                                                                                //            uint32_t                    _size;
            bool                        completeFrame_;                         // 27
            PacketMetadata               metadata_;                             // 39
            uint16_t                    bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;    // 41
        } __attribute__((packed));
        
        webrtc::EncodedImage frame_;
        unsigned int segmentSize_;
        
        void
        initialize(const webrtc::EncodedImage &frame,
                   unsigned int segmentSize);
        
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
            
            unsigned int
            getLength() { return sizeof(this->isRTCP_) +
                            sizeof(this->length_) + length_; }
            
        } __attribute__((packed)) AudioPacket;
        
        NdnAudioData(unsigned int dataLength, const unsigned char* rawData);
        NdnAudioData(){}
        ~NdnAudioData(){}
        
        PacketDataType
        getType() { return TypeAudio; }
        
        PacketMetadata
        getMetadata();
        
        void
        setMetadata(PacketMetadata& metadata);
        
        int
        initFromRawData(unsigned int dataLength, const unsigned char* rawData);
        
        std::vector<AudioPacket>&
        getPackets()
        { return packets_; }
        
        void
        clear()
        {
            packets_.clear();
            length_ = 0;
            delete data_;
            data_ = NULL;
            isDataCopied_ = false;
            isValid_ = false;
        }
        
        void
        addPacket(AudioPacket& packet);
        
        static bool
        isValidHeader(unsigned int length, const unsigned char* data);
        
        static PacketMetadata
        metadataFromRaw(unsigned int length, const unsigned char* data);
        
    private:
        struct AudioDataHeader {
            uint16_t        headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
            uint8_t             nPackets_;
            PacketMetadata      metadata_;
            uint16_t        bodyMarker_  = NDNRTC_AUDIOBODY_MRKR;
        } __attribute__((packed));
        
        std::vector<AudioPacket> packets_;
        
        AudioDataHeader
        getHeader()
        {
            return *((AudioDataHeader*)(&data_[0]));
        }
    };
    
    /**
     * Session info metadata stores all the necessary information for consumer 
     * about producer's audio/video streams and its' parameters.
     * Packed session info structure:
     * {
     *      <header marker - 2 bytes>
     *      <number of video streams - 4 bytes>
     *      <number of audio streams - 4 bytes>
     *      <session prefix string>\0
     *      <header marker - 2 bytes>
     *      [
     *          {
     *              <video stream marker - 2 bytes>
     *              <segment size - 4 bytes>
     *              <stream name - MAX_STREAM_NAME_LENGTH+1 bytes>
     *              <synchronized stream name - MAX_STREAM_NAME_LENGTH+1 bytes>
     *              <number of threads - 4 bytes>
     *              <video stream marker - 2 bytes>
     *              [
     *                  {
     *                      <video thread marker - 2 bytes>
     *                      <FPS - 4 bytes>
     *                      <GOP - 4 bytes>
     *                      <bitrate - 4 bytes>
     *                      <width - 4 bytes>
     *                      <height - 4 bytes>
     *                      <average number of delta frames - 4 bytes>
     *                      <average number of delta parity frames - 4 bytes>
     *                      <average number of key frames - 4 bytes>
     *                      <average number of key parity frames - 4 bytes>
     *                      <thread name - MAX_THREAD_NAME_LENGTH+1 bytes>
     *                      <video thread marker - 2 bytes>
     *                  },
     *                  ...
     *              ]
     *          },
     *          ...
     *      ],
     *      [
     *          {
     *              <audio stream marker - 2 bytes>
     *              <segment size - 4 bytes>
     *              <stream name - MAX_STREAM_NAME_LENGTH+1 bytes>
     *              <number of threads - 4 bytes>
     *              <audio stream marker - 2 bytes>
     *              [
     *                  {
     *                      <audio thread marker - 2 bytes>
     *                      <FPS - 4 bytes>
     *                      <thread name - MAX_THREAD_NAME_LENGTH+1 bytes>
     *                      <audio thread marker - 2 bytes>
     *                  },
     *                  ...
     *              ]
     *          },
     *          ...
     *      ]
     * }
     */
    class SessionInfoData : public NetworkData
    {
    public:
        SessionInfoData(const new_api::SessionInfo& sessionInfo);
        SessionInfoData(unsigned int dataLength, const unsigned char* data);
        virtual ~SessionInfoData() {}
        
        int
        getSessionInfo(new_api::SessionInfo& sessionInfo);

    private:
        struct _VideoThreadDescription {
            uint16_t mrkr1_ = NDNRTC_VTHREADDESC_MRKR;
            double rate_; // FPS
            unsigned int gop_;
            unsigned int bitrate_; // kbps
            unsigned int width_, height_; // pixels
            double deltaAvgSegNum_, deltaAvgParitySegNum_;
            double keyAvgSegNum_, keyAvgParitySegNum_;
            char name_[MAX_THREAD_NAME_LENGTH+1];
            uint16_t mrkr2_ = NDNRTC_VTHREADDESC_MRKR;
        } __attribute__((packed));
        
        struct _AudioThreadDescription {
            uint16_t mrkr1_ = NDNRTC_ATHREADDESC_MRKR;
            double rate_;
            char name_[MAX_THREAD_NAME_LENGTH+1];
            uint16_t mrkr2_ = NDNRTC_ATHREADDESC_MRKR;
        } __attribute__((packed));
        
        struct _VideoStreamDescription {
            uint16_t mrkr1_ = NDNRTC_VSTREAMDESC_MRKR;
            unsigned int segmentSize_;
            char name_[MAX_STREAM_NAME_LENGTH+1];
            char syncName_[MAX_STREAM_NAME_LENGTH+1];
            unsigned int nThreads_;
            uint16_t mrkr2_ = NDNRTC_VSTREAMDESC_MRKR;
        } __attribute__((packed));
        
        struct _AudioStreamDescription {
            uint16_t mrkr1_ = NDNRTC_ASTREAMDESC_MRKR;
            unsigned int segmentSize_;
            char name_[MAX_STREAM_NAME_LENGTH+1];
            unsigned int nThreads_;
            uint16_t mrkr2_ = NDNRTC_ASTREAMDESC_MRKR;
        } __attribute__((packed));
        
        struct _SessionInfoDataHeader {
            uint16_t mrkr1_ = NDNRTC_SESSION_MRKR;
            unsigned int nVideoStreams_;
            unsigned int nAudioStreams_;
            uint16_t mrkr2_ = NDNRTC_SESSION_MRKR;
        } __attribute__((packed));
        
        new_api::SessionInfo sessionInfo_;
        
        unsigned int
        getSessionInfoLength(const new_api::SessionInfo& sessionInfo);
        
        void
        packParameters(const new_api::SessionInfo& sessionInfo);
        
        int
        initFromRawData(unsigned int dataLength, const unsigned char* data);
    };
    
};

#endif /* defined(__ndnrtc__frame_slot__) */
