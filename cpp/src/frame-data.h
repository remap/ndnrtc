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
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
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

    struct Mutable {
        typedef std::vector<uint8_t> storage;
        typedef std::vector<uint8_t>::iterator payload_iter;
    };

    struct Immutable {
        typedef boost::shared_ptr<const std::vector<uint8_t>> storage;
        typedef std::vector<uint8_t>::const_iterator payload_iter;
    };

    #define ENABLE_IF(T, M) template<typename U = T, typename boost::enable_if<typename boost::is_same<M,U>>::type... X>
    #define ENABLE_FOR(M) typename boost::enable_if<typename boost::is_same<M,U>>::type* dummy = 0

    //******************************************************************************
    template<typename T = Mutable>
    class NetworkDataT {
    public:
        ENABLE_IF(T,Immutable)
        NetworkDataT(const boost::shared_ptr<const std::vector<uint8_t>>& data):data_(data){}

        ENABLE_IF(T,Mutable)
        NetworkDataT(unsigned int dataLength, const uint8_t* rawData):
            isValid_(true)
        { copyFromRaw(dataLength, rawData); }

        ENABLE_IF(T,Mutable)
        NetworkDataT(const std::vector<uint8_t>& data):isValid_(true), data_(data){}
        
        ENABLE_IF(T,Mutable)
        NetworkDataT(const NetworkDataT& data):data_(data.data_), isValid_(data.isValid_){}
        
        ENABLE_IF(T,Mutable)
        NetworkDataT(NetworkDataT&& data):isValid_(data.isValid())
        {
            data_.swap(data.data_);
            data.isValid_ = false; 
        }

        ENABLE_IF(T,Mutable)
        NetworkDataT(std::vector<uint8_t>& data):data_(boost::move(data)), isValid_(true){}
        
        virtual ~NetworkDataT() {}

        virtual int 
        getLength() const { return _data().size(); }

        const uint8_t*
        getData() const { return _data().data(); }

        const std::vector<uint8_t>& data() const 
        { return _data(); }

        bool 
        isValid() const { return isValid_; }

        ENABLE_IF(T,Mutable)
        NetworkDataT& operator=(const NetworkDataT& networkData)
        {
            if (this != &networkData)
            {
                data_ = networkData.data_;
                isValid_ = networkData.isValid_;
            }

            return *this;
        }

        void 
        swap(NetworkDataT& networkData)
        {
            std::swap(isValid_, networkData.isValid_);
            data_.swap(networkData.data_);
        }

        ENABLE_IF(T,Mutable)
        int
        getCrcValue() const
        {
            boost::crc_16_type crc_computer_;
            crc_computer_ = std::for_each(data_.begin(), data_.end(), crc_computer_);
            return crc_computer_();
        }

    protected:
        bool isValid_;
        typename T::storage data_;

        ENABLE_IF(T,Immutable)
        const std::vector<uint8_t>& _data(ENABLE_FOR(Immutable)) const 
        { return *data_; }

        ENABLE_IF(T,Mutable)
        const std::vector<uint8_t>& _data(ENABLE_FOR(Mutable)) const 
        { return data_; }

        ENABLE_IF(T,Mutable)
        std::vector<uint8_t>& _data(ENABLE_FOR(Mutable))
        { return data_; }

        ENABLE_IF(T,Mutable)
        void
        copyFromRaw(unsigned int dataLength, const uint8_t* rawData)
        { data_.assign(rawData, rawData+dataLength); }
    };

    typedef NetworkDataT<Immutable> ImmutableNetworkData;
    typedef NetworkDataT<> NetworkData;

    //******************************************************************************
    template<typename T = Mutable>
    class DataPacketT : public NetworkDataT<T>
    {
        ENABLE_IF(T,Mutable)
        DataPacketT(DataPacketT<T>&& dataPacket) = delete;

    public:
        class Blob {
        public:
            size_t size() const { return (end_-begin_); }
            uint8_t operator[](size_t pos) const { return *(begin_+pos); }
            const uint8_t* data() const { return &(*begin_); }

            Blob(const Blob& b):begin_(b.begin_), end_(b.end_){}
            Blob(const std::vector<uint8_t>::const_iterator& begin, 
                const std::vector<uint8_t>::const_iterator& end):
                begin_(begin), end_(end){}
            Blob& operator=(const Blob& b)
            {
                if (this != &b)
                {
                    begin_ = b.begin_;
                    end_ = b.end_;
                }

                return *this;
            }

            std::vector<uint8_t>::const_iterator begin() const 
            { return begin_; }
            std::vector<uint8_t>::const_iterator end() const
            { return end_; }
        protected:
            std::vector<uint8_t>::const_iterator begin_, end_;

            Blob() = delete;
        };

        DataPacketT(const DataPacketT<T>& dataPacket):
            NetworkDataT<T>(dataPacket.data_)
        { this->reinit(); }

        ENABLE_IF(T,Immutable)
        DataPacketT(const boost::shared_ptr<const std::vector<uint8_t>>& data):
            NetworkDataT<T>(data){ this->isValid_ = true; this->reinit(); }

        ENABLE_IF(T,Mutable)
        DataPacketT(unsigned int dataLength, const uint8_t* payload):
            NetworkDataT<T>(dataLength, payload)
        {
            this->_data().insert(this->_data().begin(), 0);
            payloadBegin_ = this->_data().begin()+1;
        }

        ENABLE_IF(T,Mutable)
        DataPacketT(const std::vector<uint8_t>& payload):
            NetworkDataT<T>(payload)
        {
            this->_data().insert(this->_data().begin(), 0);
            payloadBegin_ = this->_data().begin()+1;
        }

        ENABLE_IF(T,Mutable)
        DataPacketT(NetworkDataT<T>&& networkData):
            NetworkDataT<T>(boost::move(networkData))
        { this->reinit(); }

        const Blob getPayload() const
        { return Blob(payloadBegin_, this->_data().end()); }

        ENABLE_IF(T,Mutable)
        void swap(NetworkDataT<T>& networkData) 
        { NetworkDataT<T>::swap(networkData); this->reinit(); }

        void swap(DataPacketT<T>& dataPacket)
        { this->data_.swap(dataPacket.data_); this->reinit(); dataPacket.reinit(); }

        /**
         * This calculates total wire length for a packet with given 
         * payloadLength and one blob (header) of length blobLength
         */
        static size_t wireLength(size_t payloadLength, size_t blobLength)
        {
            size_t wireLength = 1+payloadLength;
            if (blobLength > 0) wireLength += 2+blobLength;
            return wireLength;
        }

        /**
         * This calculates total wire length for a packet with given
         * payloadLength and arbitrary number of blobs (headers), lengths
         * of which are given in vector blobLengths 
         */
        static size_t wireLength(size_t payloadLength, std::vector<size_t>  blobLengths)
        {
            size_t wireLength = 1+payloadLength;
            for (auto b:blobLengths) if (b>0) wireLength += 2+b;
                return wireLength;
        }
        /**
         * This calculates length of byte overhead for adding one blob (header)
         * of length blobLength to a data packet
         */
        static size_t wireLength(size_t blobLength)
        {
            if (blobLength) return blobLength+2;
            return 0;
        }

        /**
         * This calculates length of byte overhead for adding arbitrary number
         * of blobs (headers) to a data packet, lenghts of which are given 
         * in vector blobLengths
         */
        static size_t wireLength(std::vector<size_t>  blobLengths)
        {
            size_t wireLength = 0;
            for (auto b:blobLengths) wireLength += 2+b;
                return wireLength;
        }

    protected:
        std::vector<Blob> blobs_;
        typename T::payload_iter payloadBegin_;

        unsigned int getBlobsNum() const { return blobs_.size(); }
        const Blob getBlob(size_t pos) const { return blobs_[pos]; }

        virtual void reinit()
        {
            blobs_.clear();
            if (!this->_data().size()) { this->isValid_ = false; return; }

            typename T::payload_iter p1 = (this->_data().begin()+1), p2;
            uint8_t nBlobs = this->_data()[0];
            bool invalid = false;

            for (int i = 0; i < nBlobs; i++)
            {
                uint8_t b1 = *p1++, b2 = *p1;
                uint16_t blobSize = b1|((uint16_t)b2)<<8;

                if (p1-this->_data().begin()+blobSize > this->_data().size())
                {
                    invalid = true;
                    break;
                }

                p2 = ++p1+blobSize;
                blobs_.push_back(Blob(p1,p2));
                p1 = p2;
            }

            if (!invalid) payloadBegin_ = p1;
            else this->isValid_ = false;
        }

        ENABLE_IF(T,Mutable)
        void addBlob(uint16_t dataLength, const uint8_t* data)
        {
            if (dataLength == 0) return;

            // increase blob counter
            this->_data()[0]++;
            // save blob size
            uint8_t b1 = dataLength&0x00ff, b2 = (dataLength&0xff00)>>8;
            payloadBegin_ = this->_data().insert(payloadBegin_, b1);
            payloadBegin_++;
            payloadBegin_ = this->_data().insert(payloadBegin_, b2);
            payloadBegin_++;
            // insert blob
            this->_data().insert(payloadBegin_, data, data+dataLength);
            this->reinit();
        }
    };

    typedef DataPacketT<Immutable> ImmutableDataPacket;
    typedef DataPacketT<> DataPacket;

    //******************************************************************************
    template<typename Header, typename T>
    class HeaderPacketT : public DataPacketT<T> {
    public:
        ENABLE_IF(T,Immutable)
        HeaderPacketT(const boost::shared_ptr<const std::vector<uint8_t>>& data):
            DataPacketT<T>(data)
        {
            isHeaderSet_ = this->isValid_ && (this->blobs_.size() >= 1) && 
                    (this->blobs_.back().size() == sizeof(Header));
            this->isValid_ = isHeaderSet_;
        }

        ENABLE_IF(T,Mutable)
        HeaderPacketT(unsigned int dataLength, const uint8_t* payload):
            DataPacketT<T>(dataLength, payload),
                isHeaderSet_(false){ this->isValid_ = false; }
        
        ENABLE_IF(T,Mutable)
        HeaderPacketT(const std::vector<uint8_t>& payload):
            DataPacketT<T>(payload),
            isHeaderSet_(false){ this->isValid_ = false; }
        
        ENABLE_IF(T,Mutable)
        HeaderPacketT(const Header& header, unsigned int dataLength, 
            const uint8_t* payload): 
            DataPacketT<T>(dataLength, payload),
            isHeaderSet_(false)
            { setHeader(header); }
        
        ENABLE_IF(T,Mutable)
        HeaderPacketT<T>(NetworkData&& networkData):
            DataPacket(boost::move(networkData))
            {
                if (this->blobs_.size() > 0)
                {
                    this->isValid_ = (this->blobs_.back().size() == sizeof(Header));
                    isHeaderSet_ = true;
                }
            }

        ENABLE_IF(T,Mutable)
        void setHeader(const Header& header)
        {
            if (!isHeaderSet_)
            {
                this->addBlob(sizeof(header), (uint8_t*)&header);
                this->isValid_ = true;
                isHeaderSet_ = true;
            }
            else
                throw std::runtime_error("Sample header has been already "
                    "added to the packet");
        }

        const Header& getHeader() const
        { return *((Header*)this->blobs_.back().data()); }

        void swap(HeaderPacketT<Header,T>& packet)
        { DataPacketT<T>::swap((DataPacketT<T>&)packet); std::swap(isHeaderSet_, packet.isHeaderSet_); }

    protected:
        bool isHeaderSet() const { return isHeaderSet_; }

    private:
        bool isHeaderSet_;
    };

    template<typename Header>
    using HeaderPacket = HeaderPacketT<Header, Mutable>;

    template<typename Header>
    using ImmutableHeaderPacket = HeaderPacketT<Header, Immutable>;

    /*******************************************************************************
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

    typedef HeaderPacket<CommonHeader> CommonSamplePacket;

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

    /*******************************************************************************
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
            boost::shared_ptr<HeaderPacket<Header>> sp =
                boost::make_shared<HeaderPacket<Header>>(std::vector<uint8_t>(begin_, end_));
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

    /*******************************************************************************
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

        static boost::shared_ptr<VideoFramePacket> 
        merge(const std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>>&);

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

    /*******************************************************************************
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
        static boost::shared_ptr<AudioBundlePacket> merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>>&);

    private:
        AudioBundlePacket(AudioBundlePacket&& bundle) = delete;

        size_t wireLength_, remainingSpace_;

        static size_t payloadLength(size_t wireLength);
    };

    //******************************************************************************
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

    //******************************************************************************
    class WireSegment {
    public:
        WireSegment(const boost::shared_ptr<ndn::Data>& data):data_(data),
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

        WireSegment(const WireSegment& data):data_(data.data_),
            dataNameInfo_(data.dataNameInfo_), isValid_(data.isValid_)
        {}

        virtual ~WireSegment(){}

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

        const NamespaceInfo& getInfo() const { return dataNameInfo_; }

        const DataSegmentHeader header() 
        {
            // this packet will be invalid for VideoFrameSegment packets,
            // still casting to DataSegmentHeader is possible, because 
            // it's a parent class for VideoFrameSegmentHeader
            ImmutableHeaderPacket<DataSegmentHeader> s(data_->getContent());
            return s.getHeader();
        }

    protected:
        NamespaceInfo dataNameInfo_;
        bool isValid_;
        boost::shared_ptr<ndn::Data> data_;
    };

    template<typename SegmentHeader>
    class WireData : public WireSegment 
    {
    public:
        WireData(const boost::shared_ptr<ndn::Data>& data):WireSegment(data){}
        WireData(const WireData<SegmentHeader>& data):WireSegment(data){}

        const ImmutableHeaderPacket<SegmentHeader> segment() const
        {
            return ImmutableHeaderPacket<SegmentHeader>(data_->getContent());
        }
    };

};

#endif /* defined(__ndnrtc__frame_slot__) */
