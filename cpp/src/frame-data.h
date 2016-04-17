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
#include <webrtc/video_frame.h>
#include <ndn-cpp/name.hpp>
#include <boost/move/move.hpp>
#include <map>

#include "webrtc.h"
#include "params.h"
#include "ndnrtc-common.h"

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
    
    #if 0
    /**
     * This stucture is used for storing meta info carried by data name prefixes
     */
    typedef std::vector<std::pair<std::string, PacketNumber>> ThreadSyncList;
    typedef struct _PrefixMetaInfo {
        int totalSegmentsNum_;
        PacketNumber playbackNo_;
        PacketNumber pairedSequenceNo_;
        int paritySegmentsNum_;
        int crcValue_;
        ThreadSyncList syncList_;
        
        static const std::string SyncListMarker;
        static const _PrefixMetaInfo ZeroMetaInfo;
        
        static ndn::Name toName(const _PrefixMetaInfo &meta);
        static int extractMetadata(const ndn::Name& metaComponents, _PrefixMetaInfo &meta);
        
        _PrefixMetaInfo();
        ~_PrefixMetaInfo(){}
    private:
        static ThreadSyncList extractSyncList(const ndn::Name &prefix, int markerPos);
        
    } __attribute__((packed)) PrefixMetaInfo DEPRECATED;
    #endif
    
    /**
     * Base data class used for network communication
     */
    class NetworkData
    {
    public:
        NetworkData(unsigned int dataLength, const uint8_t* rawData);
        NetworkData(const std::vector<uint8_t>& data);
        NetworkData(const NetworkData& networkData);
        NetworkData(NetworkData&& networkData);
        NetworkData(std::vector<uint8_t>& data);
        virtual ~NetworkData() {}

        NetworkData& operator=(const NetworkData& networkData);

        virtual void 
        swap(NetworkData& networkData);

        virtual int
        getLength() const { return data_.size(); }
        
        const uint8_t*
        getData() const { return data_.data(); }

        virtual const std::vector<uint8_t>& data() const { return data_; }
        
        virtual bool
        isValid() const { return isValid_; }
        
        int
        getCrcValue() const
        {
            boost::crc_16_type crc_computer_;
            crc_computer_ = std::for_each(data_.begin(), data_.end(), crc_computer_);
            return crc_computer_();
        }
        
    protected:
        NetworkData() = delete;

        bool isValid_ = false;
        std::vector<uint8_t> data_;
        
        virtual int
        initFromRawData(unsigned int dataLength,
                        const uint8_t* rawData) { return 0; };
        
        void
        copyFromRaw(unsigned int dataLength, const uint8_t* rawData);
    };

    /**
     * General data packet used by application to pack binary data into a 
     * NetworkData object. Provides interface for adding arbitrary number of
     * data headers (represented by NetworkData objects). Unlike NetworkData,
     * DataPacket maintains a packet format and if one will construct DataPacket
     * from arbitrary raw data, calling isValid() on constructed object will 
     * return false. Data buffer in this case will be empty (getLength will 
     * return 0). The DataPacket format is simple - headers are separated by 
     * special markers and the rest of the packet is the DataPacket payload.
     */
    class DataPacket : public NetworkData
    {
    public:
        class Blob {
        public:
            size_t size() const;
            uint8_t operator[](size_t pos) const;
            const uint8_t* data() const;

            Blob(const Blob& b):begin_(b.begin_), end_(b.end_){}
            Blob(const std::vector<uint8_t>::const_iterator& begin, 
                 const std::vector<uint8_t>::const_iterator& end);
            Blob& operator=(const Blob& b);

            std::vector<uint8_t>::const_iterator begin() const 
            { return begin_; }
            std::vector<uint8_t>::const_iterator end() const
            { return end_; }
        protected:
            std::vector<uint8_t>::const_iterator begin_, end_;

            Blob() = delete;
        };

        DataPacket(unsigned int dataLength, const uint8_t* payload);
        DataPacket(const std::vector<uint8_t>& payload);
        DataPacket(const DataPacket& dataPacket);
        DataPacket(NetworkData&& networkData);

        const Blob getPayload() const;

        /**
         * This calculates total wire length for a packet with given 
         * payloadLength and one blob (header) of length blobLength
         */
        static size_t wireLength(size_t payloadLength, size_t blobLength);
        /**
         * This calculates total wire length for a packet with given
         * payloadLength and arbitrary number of blobs (headers), lengths
         * of which are given in vector blobLengths 
         */
        static size_t wireLength(size_t payloadLength, std::vector<size_t>  blobLengths);
        /**
         * This calculates length of byte overhead for adding one blob (header)
         * of length blobLength to a data packet
         */
        static size_t wireLength(size_t blobLength);
        /**
         * This calculates length of byte overhead for adding arbitrary number
         * of blobs (headers) to a data packet, lenghts of which are given 
         * in vector blobLengths
         */
        static size_t wireLength(std::vector<size_t>  blobLengths);

     protected:
        std::vector<Blob> blobs_;
        std::vector<uint8_t>::iterator payloadBegin_;
        size_t payloadSize_;

        unsigned int getBlobsNum() const { return blobs_.size(); }
        const Blob getBlob(size_t pos) const { return blobs_[pos]; }

        // one header can't be bigger than 255 bytes
        void addBlob(uint16_t dataLength, const uint8_t* data);
        void reinit();
    };

    template <typename Header>
    class SamplePacket : public DataPacket {
    public:
        SamplePacket(unsigned int dataLength, const uint8_t* payload):
            DataPacket(dataLength, payload),
            isHeaderSet_(false){ isValid_ = false; }
        SamplePacket(const std::vector<uint8_t>& payload):
            DataPacket(payload),
            isHeaderSet_(false){ isValid_ = false; }
        SamplePacket(const Header& header, unsigned int dataLength, 
            const uint8_t* payload): 
            DataPacket(dataLength, payload),
            isHeaderSet_(false)
            { setHeader(header); }
        SamplePacket(NetworkData&& networkData):
            DataPacket(boost::move(networkData))
            {
                if (isValid_ && blobs_.size() > 0)
                {
                    isValid_ = (blobs_.back().size() == sizeof(Header));
                    isHeaderSet_ = true;
                }
            }

        virtual void setHeader(const Header& header)
        {
            if (!isHeaderSet_)
            {
                addBlob(sizeof(header), (uint8_t*)&header);
                isValid_ = true;
                isHeaderSet_ = true;
            }
            else
                throw std::runtime_error("Sample header has been already "
                    "added to the packet");
        }

        virtual const Header& getHeader() const
        { return *((Header*)blobs_.back().data()); }
    
    protected:
        bool isHeaderSet() { return isHeaderSet_; }
    private:
        SamplePacket(const SamplePacket&) = delete;
        
        bool isHeaderSet_;
    };

    typedef struct _CommonHeader {
            double sampleRate_; // current packet production rate
            int64_t publishTimestampMs_; // packet timestamp set by producer
            double publishUnixTimestampMs_; // unix timestamp set by producer
    } __attribute__((packed)) CommonHeader;
    
    typedef struct _AudioSampleHeader {
        bool isRtcp_;
    } __attribute__((packed)) AudioSampleHeader;

    typedef SamplePacket<CommonHeader> CommonSamplePacket;
    typedef SamplePacket<AudioSampleHeader> AudioSamplePacket;

    class VideoFramePacket : public CommonSamplePacket {
    public:
        typedef std::map<std::string, PacketNumber> ThreadSyncList;
        
        VideoFramePacket(const webrtc::EncodedImage& frame);
        VideoFramePacket(NetworkData&& networkData);

        const webrtc::EncodedImage& getFrame();
        boost::shared_ptr<NetworkData> 
            getParityData(size_t segmentSize, double ratio);
        void setSyncList(const ThreadSyncList& syncList);
        const std::map<std::string, PacketNumber> getSyncList() const;

    private:
        typedef struct _Header {
            uint32_t encodedWidth_;
            uint32_t encodedHeight_;
            uint32_t timestamp_;
            int64_t capture_time_ms_;
            WebRtcVideoFrameType frameType_;
            bool completeFrame_;
        } __attribute__((packed)) Header;

        webrtc::EncodedImage frame_;
        bool isSyncListSet_;
    };

    class AudioBundlePacket : public CommonSamplePacket
    {
    public:
        class AudioSampleBlob : protected DataPacket::Blob {
        public:
            AudioSampleBlob(const AudioSampleHeader& hdr, size_t sampleLen, const uint8_t* data):
                Blob(std::vector<uint8_t>::const_iterator(data), 
                    std::vector<uint8_t>::const_iterator(data+sampleLen)), 
                header_(hdr), fromBlob_(false){}
            AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
                const std::vector<uint8_t>::const_iterator& end);

            const AudioSampleHeader& getHeader() const { return header_; }
            size_t size() const;
            const uint8_t* data() const { return (fromBlob_?&(*(begin_+sizeof(AudioSampleHeader))):&(*begin_)); }
            // uint8_t operator[](size_t pos) const { return Blob::operator[](pos+(fromBlob_?sizeof(AudioSampleHeader):0)); };

            static size_t wireLength(size_t payloadLength);
        private:
            bool fromBlob_;
            AudioSampleHeader header_;
        };

        AudioBundlePacket(size_t wireLength);
        AudioBundlePacket(NetworkData&& networkData);

        bool hasSpace(const AudioSampleBlob& sampleBlob) const;
        size_t getRemainingSpace() const { return remainingSpace_; }
        void clear();
        AudioBundlePacket& operator<<(const AudioSampleBlob& sampleBlob);
        size_t getSamplesNum();
        const AudioSampleBlob operator[](size_t pos) const;
        
        static size_t wireLength(size_t wireLength, size_t sampleSize);
    private:
        size_t wireLength_, remainingSpace_;

        static size_t payloadLength(size_t wireLength);
    };

    typedef struct _DataSegmentHeader {
        int32_t interestNonce_;
        double interestArrivalMs_;
        double generationDelayMs_;

        _DataSegmentHeader():interestNonce_(0),
            interestArrivalMs_(0), generationDelayMs_(0){}
    } __attribute__((packed)) DataSegmentHeader;
    
    typedef struct _VideoFrameSegmentHeader : public DataSegmentHeader {
        int totalSegmentsNum_;
        PacketNumber playbackNo_;
        PacketNumber pairedSequenceNo_;
        int paritySegmentsNum_;

        _VideoFrameSegmentHeader():totalSegmentsNum_(0), playbackNo_(0),
            pairedSequenceNo_(0), paritySegmentsNum_(0){}
    } __attribute__((packed)) VideoFrameSegmentHeader;

    /**
     * This class represents data segment used for publishing NDN data. Unlike 
     * DataPacket, it doesn't have it's own storage - instead, it must be 
     * constructed using existing data. This restriction represents the assumption
     * that data segments can't be created without corresponding data packet. 
     * Usually, one would use slice() static method to generate a vector of 
     * segments that altogether represent given data packet. As data segment 
     * doesn't carry additional storage, it's leightweight. A copy of NetworkData
     * is created when getNetworkData() is called. This must be called right before
     * transferring data over the wire (publishing).
     * Class can be instantiated with different headers.
     * @see slice()
     * @see VideoFrameSegment
     * @see CommonSegment
     */
    template <typename Header>
    class DataSegment : protected DataPacket::Blob {
    public:
        DataSegment(const DataSegment& s):Blob(s), header_(s.header_){}
        DataSegment(const std::vector<uint8_t>::const_iterator& begin, 
           const std::vector<uint8_t>::const_iterator& end):Blob(begin, end)
        { memset(&header_, 0, sizeof(header_)); }
        
        void setHeader(const DataSegmentHeader& header) { header_ = reinterpret_cast<const Header&>(header); }
        const Header& getHeader() const { return header_; }
        const DataPacket::Blob& getPayload() const { return *this; }
        size_t size() const { return DataSegment<Header>::wireLength(Blob::size()); }

        /**
         * Creates new NetworkData object which has data copied using begin 
         * and end iterators passed to this object during creation
         */
        const boost::shared_ptr<NetworkData> getNetworkData() const
        {
            boost::shared_ptr<SamplePacket<Header>> sp =
                boost::make_shared<SamplePacket<Header>>(std::vector<uint8_t>(begin_, end_));
            sp->setHeader(header_);
            return sp;
        }

        /**
         * This calculates total wire length for a segment with given payload 
         * length
         */
        static size_t wireLength(size_t payloadLength)
        {
            return DataPacket::wireLength(payloadLength, sizeof(Header));
        }
        /**
         * This calculates maximum payload length for a given target wire length 
         * of a data segment
         */
        static size_t payloadLength(size_t wireLength)
        {   
            long payloadLength = wireLength-1-DataPacket::wireLength(sizeof(Header));
            return (payloadLength < 0)?0:payloadLength;
        }

        static size_t numSlices(const NetworkData& nd, 
            size_t segmentWireLength)
        {
            size_t payloadLength = DataSegment<Header>::payloadLength(segmentWireLength);
            return (nd.getLength()/payloadLength) + (nd.getLength()%payloadLength?1:0);
        }

        static std::vector<DataSegment<Header>> slice(const NetworkData& nd, 
            size_t segmentWireLength)
        {
            std::vector<DataSegment<Header>> segments;
            size_t payloadLength = DataSegment<Header>::payloadLength(segmentWireLength);

            if (payloadLength == 0)
                return segments;

            std::vector<uint8_t>::const_iterator p1 = nd.data().begin();
            std::vector<uint8_t>::const_iterator p2 = p1+payloadLength;

            while(p2 < nd.data().end()){
                segments.push_back(DataSegment<Header>(p1,p2));
                p1 = p2;
                p2 += payloadLength;
            }

            segments.push_back(DataSegment<Header>(p1,nd.data().end()));

            return segments;
        }

    private:
        Header header_;
    };

    typedef DataSegment<VideoFrameSegmentHeader> VideoFrameSegment;
    typedef DataSegment<DataSegmentHeader> CommonSegment;

    #if 0
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
    #if 0
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
    #endif
    
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
    #endif
};

#endif /* defined(__ndnrtc__frame_slot__) */
