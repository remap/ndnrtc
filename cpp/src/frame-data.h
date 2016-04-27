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
#include "name-components.h"

namespace ndn{
    class Data;
};

namespace ndnrtc {

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

        virtual void swap(NetworkData& networkData) 
        { NetworkData::swap(networkData); reinit(); }

        virtual void swap(DataPacket& dataPacket)
        { NetworkData::swap(dataPacket); reinit(); dataPacket.reinit(); }

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

        unsigned int getBlobsNum() const { return blobs_.size(); }
        const Blob getBlob(size_t pos) const { return blobs_[pos]; }

        void addBlob(uint16_t dataLength, const uint8_t* data);
        virtual void reinit();

    private:
        DataPacket(DataPacket&& dataPacket) = delete;
    };

    /**
     * SamplePacket represents data packet for storing sample data - video frame
     * or audio samples. When initialized with proper segment header class, it
     * provides functionality to prepare sample data for publishing. It is a base
     * class for VideoFramePacket and AudioBundlePacket classes.
     */
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
                if (blobs_.size() > 0)
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

        virtual void swap(SamplePacket& packet)
        { DataPacket::swap(packet); std::swap(isHeaderSet_, packet.isHeaderSet_); }
    
    protected:
        bool isHeaderSet() const { return isHeaderSet_; }
    private:
        SamplePacket(const SamplePacket&) = delete;
        
        bool isHeaderSet_;
    };

    /**
     * Common sample header used as a header for audio sample packets and parity
     * data packets.
     */
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

    /**
     * VideoFramePacket provides interface for preparing encoded video frame for
     * publishing as a data packet.
     */
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

    /**
     * AudioBundlePacket provides interface for bundling audio samples and 
     * preparing them for publishing as a data packet.
     */
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

            static size_t wireLength(size_t payloadLength);
        private:
            bool fromBlob_;
            AudioSampleHeader header_;
        };

        AudioBundlePacket(size_t wireLength);
        AudioBundlePacket(NetworkData&& bundle);

        bool hasSpace(const AudioSampleBlob& sampleBlob) const;
        size_t getRemainingSpace() const { return remainingSpace_; }
        void clear();
        AudioBundlePacket& operator<<(const AudioSampleBlob& sampleBlob);
        size_t getSamplesNum() const;
        const AudioSampleBlob operator[](size_t pos) const;
        void swap(AudioBundlePacket& bundle);
        
        static size_t wireLength(size_t wireLength, size_t sampleSize);
    private:
        AudioBundlePacket(AudioBundlePacket&& bundle) = delete;

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

        static size_t headerSize(){ return sizeof(Header); }

    private:
        Header header_;
    };

    typedef DataSegment<VideoFrameSegmentHeader> VideoFrameSegment;
    typedef DataSegment<DataSegmentHeader> CommonSegment;

    class AudioThreadMeta : public DataPacket
    {
    public:
        AudioThreadMeta(double rate, const std::string& codec);
        AudioThreadMeta(NetworkData&& data);

        std::string getCodec() const;
        double getRate() const;
    };

    class VideoThreadMeta : public DataPacket
    {
    public:
        VideoThreadMeta(double rate, const FrameSegmentsInfo& segInfo,
            const VideoCoderParams& coder);
        VideoThreadMeta(NetworkData&& data);

        double getRate() const;
        FrameSegmentsInfo getSegInfo() const;
        VideoCoderParams getCoderParams() const;

    private:
        typedef struct _Meta {
            double rate_; // FPS
            unsigned int gop_;
            unsigned int bitrate_; // kbps
            unsigned int width_, height_; // pixels
            double deltaAvgSegNum_, deltaAvgParitySegNum_;
            double keyAvgSegNum_, keyAvgParitySegNum_;
        } __attribute__((packed)) Meta;
    };

    class MediaStreamMeta : public DataPacket
    {
    public:
        MediaStreamMeta();
        MediaStreamMeta(std::vector<std::string> threads);
        MediaStreamMeta(NetworkData&& nd):DataPacket(boost::move(nd)){}

        void addThread(const std::string& threadName);
        void addSyncStream(const std::string& syncStream);
        std::vector<std::string> getSyncStreams() const;
        std::vector<std::string> getThreads() const;
    };

    template<typename SegmentType>
    class WireData {
    public:
        WireData(const boost::shared_ptr<ndn::Data>& data):data_(data),
            isValid_(NameComponents::extractInfo(data->getName(), dataNameInfo_))
        {
            if (dataNameInfo_.apiVersion_ != NameComponents::nameApiVersion())
            {
                std::stringstream ss;
                ss << "Attempt to create wired data object with "
                    << "unsupported namespace API version: " << dataNameInfo_.apiVersion_ << std::endl;
                throw std::runtime_error(ss.str());
            }
        }

        WireData(const WireData& data):data_(data.data_),
            dataNameInfo_(data.dataNameInfo_), isValid_(data.isValid_)
        {}

        ~WireData(){}

        bool isValid() { return isValid_; }
        size_t getSlicesNum()
        {
            return data_->getMetaInfo().getFinalBlockId().toNumber();
        }

        boost::shared_ptr<ndn::Data> getData() { return data_; }
        ndn::Name getBasePrefix() { return dataNameInfo_.basePrefix_; }
        unsigned int getApiVersion() { return dataNameInfo_.apiVersion_; }
        MediaStreamParams::MediaStreamType getStreamType() { return dataNameInfo_.streamType_; }
        std::string getStreamName() { return dataNameInfo_.streamName_; }
        bool isMeta() { return dataNameInfo_.isMeta_; }
        PacketNumber getSampleNo() { return dataNameInfo_.sampleNo_; }
        unsigned int getSegNo() { return dataNameInfo_.segNo_; }
        bool isParity() { return dataNameInfo_.isParity_; }
        std::string getThreadName() { return dataNameInfo_.threadName_; }

        const SegmentType& segment() const { return SegmentType(data_); }

    private:
        NamespaceInfo dataNameInfo_;
        bool isValid_;
        boost::shared_ptr<ndn::Data> data_;
    };
};

#endif /* defined(__ndnrtc__frame_slot__) */
