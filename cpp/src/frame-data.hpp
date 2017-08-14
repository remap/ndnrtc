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
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <ndn-cpp/name.hpp>
#include <ndn-cpp/data.hpp>
#include <boost/move/move.hpp>
#include <map>

#include "webrtc.hpp"
#include "params.hpp"
#include "ndnrtc-common.hpp"
#include "name-components.hpp"
#include "fec.hpp"

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
    /**
     * Network data class is a base class for network data used to transfer binary 
     * data over the (NDN) network.
     * NetworkDataT is a template class, which can be instantiated in two flavors - mutable
     * data and immutable data. The former can be used for preparing data packets to be
     * sent over the network. For example, one creates empty packet and then adds necessary
     * payload(s) to it. (One should rather use derived classes, rather than this class 
     * directly). The payload is stored as a vector of bytes and copied/moved according to
     * the constructor used.
     * The latter (immutable network data) is used for packets, received from the network.
     * It is intentional by design, that incoming packets are immutable. Internal storage
     * is a smart pointer to a vector of bytes. This allows for ImmutableNetworkData objects
     * to be leightweight when copied or passed by values.
     */
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

        /**
         * Returns packet payload size in bytes
         */
        virtual int 
        getLength() const { return _data().size(); }

        /**
         * Returns const pointer to the packet payload
         */
        const uint8_t*
        getData() const { return _data().data(); }

        /**
         * Returns payload as const vector of bytes
         */
        const std::vector<uint8_t>& data() const 
        { return _data(); }

        /**
         * Flag indicating whether the packet is valid. Primarily used by derived classes, or
         * in cases, when move constructor is used for mutable network data - packet which
         * payload was moved becomes invalid.
         */
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

        /**
         * Swaps two packets
         */
        void 
        swap(NetworkDataT& networkData)
        {
            std::swap(isValid_, networkData.isValid_);
            data_.swap(networkData.data_);
        }

        /**
         * Calculates CRC for the packet
         */
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
    /**
     * Data packet class extends NetworkData functionality by implementing addBlob
     * method whith allows to add any number (less than 255) of binary data of any
     * size (less than 65525)to the packet and retrieve it later.
     * The wire format of the data packet is the following:
     *
     *      <#_of_blobs>[<blob_size_byte0><blob_size_byte1><blob>]*<payload_bytes>+
     *
     */
    template<typename T = Mutable>
    class DataPacketT : public NetworkDataT<T>
    {
        ENABLE_IF(T,Mutable)
        DataPacketT(DataPacketT<T>&& dataPacket) = delete;

    public:
        /**
         * Blob provides read-only interface for immutable data. It is a helper
         * class which makes manipulation of data packet internals easier (such
         * manipulations as counting number of headers in data packet, their 
         * sizes, ordering, etc). Blob's constructor takes two const iterators 
         * or another Blob object. No payload copying is performed during 
         * construction.
         */
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
    /**
     * HeaderPacket extends DataPacket class by adding functionality for a header 
     * which is usually a structure.
     * Header is appended as the last blob to the packet and one can not set header
     * several times - an exception will be raised. Typically, one whould add header
     * to the packet as the very last step in preparing data to be transfered over 
     * the network.
     */
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
        HeaderPacketT(NetworkData&& networkData):
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
        void clear() 
        { 
            this->_data().clear();
            this->_data().insert(this->_data().begin(),0);
            this->payloadBegin_ = this->_data().begin()+1;
            this->blobs_.clear();
            isHeaderSet_ = false;
        }

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
    template<typename T=Mutable>
    class VideoFramePacketT : public HeaderPacketT<CommonHeader, T> {
    public:
        typedef std::map<std::string, PacketNumber> ThreadSyncList;
        
        ENABLE_IF(T,Immutable)
        VideoFramePacketT(const boost::shared_ptr<const std::vector<uint8_t>>& data):
            HeaderPacketT<CommonHeader,T>(data){}

        ENABLE_IF(T,Mutable)
        VideoFramePacketT(const webrtc::EncodedImage& frame):
            HeaderPacketT<CommonHeader,T>(frame._length, frame._buffer), 
            isSyncListSet_(false), isUserDataSet_(false)
        {
            assert(frame._encodedWidth);
            assert(frame._encodedHeight);

            Header hdr;
            hdr.encodedWidth_ = frame._encodedWidth;
            hdr.encodedHeight_ = frame._encodedHeight;
            hdr.timestamp_ = frame._timeStamp;
            hdr.capture_time_ms_ = frame.capture_time_ms_;
            hdr.frameType_ = frame._frameType;
            hdr.completeFrame_ = frame._completeFrame;
            hdr.frameLength_ = frame._length;
            this->addBlob(sizeof(hdr), (uint8_t*)&hdr);
        }
        
        ENABLE_IF(T,Mutable)
        VideoFramePacketT(NetworkData&& networkData):
            CommonSamplePacket(boost::move(networkData)){}

        const webrtc::EncodedImage& getFrame()
        {
            Header *hdr = (Header*)this->blobs_[0].data();
            int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_, 
                hdr->encodedHeight_);
            frame_ = webrtc::EncodedImage((uint8_t*)(this->_data().data()+(this->payloadBegin_-this->_data().begin())), 
                hdr->frameLength_, size);
            frame_._encodedWidth = hdr->encodedWidth_;
            frame_._encodedHeight = hdr->encodedHeight_;
            frame_._timeStamp = hdr->timestamp_;
            frame_.capture_time_ms_ = hdr->capture_time_ms_;
            frame_._frameType = hdr->frameType_;
            frame_._completeFrame = hdr->completeFrame_;

            return frame_;
        }

        const std::map<std::string, PacketNumber> getSyncList() const
        {
            typedef typename std::vector<typename DataPacketT<T>::Blob>::const_iterator BlobIterator;
            std::map<std::string, PacketNumber> syncList;

            for (BlobIterator blob = this->blobs_.begin()+1; 
                blob+1 < this->blobs_.end(); blob+=2)
                syncList[std::string((const char*)blob->data(), blob->size())] = *(PacketNumber*)(blob+1)->data();

            return boost::move(syncList);
        }

        ENABLE_IF(T,Mutable)
        boost::shared_ptr<NetworkData> 
        getParityData(size_t segmentLength, double ratio)
        {
            if (!this->isValid_)
                throw std::runtime_error("Can't compute FEC parity data on invalid packet");

            size_t nDataSegmets = this->getLength()/segmentLength + (this->getLength()%segmentLength?1:0);
            size_t nParitySegments = ceil(ratio*nDataSegmets);
            if (nParitySegments == 0) nParitySegments = 1;

            std::vector<uint8_t> fecData(nParitySegments*segmentLength, 0);
            fec::Rs28Encoder enc(nDataSegmets, nParitySegments, segmentLength);
            size_t padding =  (nDataSegmets*segmentLength - this->getLength());
            boost::shared_ptr<NetworkData> parityData;

            // expand data with zeros
            this->_data().resize(nDataSegmets*segmentLength, 0);
            if (enc.encode(this->_data().data(), fecData.data()) >= 0)
                parityData = boost::make_shared<NetworkData>(boost::move(fecData));
            // shrink data back
            this->_data().resize(this->getLength()-padding);
            this->reinit(); // data may have been relocated, so we need to reinit blobs

            return parityData;
        }

        ENABLE_IF(T,Mutable)
        void setSyncList(const ThreadSyncList& syncList)
        {
            if (this->isHeaderSet()) throw std::runtime_error("Can't add more data to this packet"
                " as header has been set already");
                if (isSyncListSet_) throw std::runtime_error("Sync list has been already set");

            for (auto it:syncList)
            {
                this->addBlob(it.first.size(), (uint8_t*)it.first.c_str());
                this->addBlob(sizeof(it.second), (uint8_t*)&it.second);
            }

            isSyncListSet_ = true;
        }

        ENABLE_IF(T,Mutable)
        void setUserData(const std::map<std::string, std::pair<unsigned int, unsigned char*>>& userData)
        {
            if (this->isHeaderSet()) throw std::runtime_error("Can't add more data to this packet"
                " as header has been set already");
            if (isUserDataSet_) throw std::runtime_error("User Data has been already set");

            for (auto it:userData)
            {
                this->addBlob(sizeof(it.second.first), (uint8_t*)&it.second.first);
                this->addBlob(it.second.first, it.second.second);
            }

            isUserDataSet_ = true;
        }

        typedef std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> ImmutableVideoSegmentsVector;
        typedef std::vector<ImmutableHeaderPacket<DataSegmentHeader>> ImmutableRecoverySegmentsVector;

        static boost::shared_ptr<VideoFramePacketT<>> 
        merge(const ImmutableVideoSegmentsVector& segments);

    private:
        typedef struct _Header {
            uint32_t encodedWidth_;
            uint32_t encodedHeight_;
            uint32_t timestamp_;
            int64_t capture_time_ms_;
            WebRtcVideoFrameType frameType_;
            bool completeFrame_;
            uint32_t frameLength_;
        } __attribute__((packed)) Header;

        webrtc::EncodedImage frame_;
        bool isSyncListSet_, isUserDataSet_;
    };

    typedef VideoFramePacketT<> VideoFramePacket;
    typedef VideoFramePacketT<Immutable> ImmutableVideoFramePacket; 

    /*******************************************************************************
     * AudioBundlePacket provides interface for bundling audio samples and 
     * preparing them for publishing as a data packet.
     */
    template<typename T>
    class AudioBundlePacketT : public HeaderPacketT<CommonHeader, T>
    {
    public:
        class AudioSampleBlob : protected DataPacket::Blob {
        public:
            AudioSampleBlob(const AudioSampleHeader& hdr, const std::vector<uint8_t>::const_iterator begin,
                    const std::vector<uint8_t>::const_iterator& end):
                Blob(begin, end), header_(hdr), fromBlob_(false){}

            AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
                    const std::vector<uint8_t>::const_iterator& end):
                Blob(begin, end), fromBlob_(true)
                {
                    header_ = *(AudioSampleHeader*)Blob::data();
                }

            const AudioSampleHeader& getHeader() const 
                { return header_; }
            size_t size() const
                { return Blob::size()+(fromBlob_?0:sizeof(AudioSampleHeader)); }
            size_t payloadLength() const
                { return Blob::size()+(fromBlob_?-sizeof(AudioSampleHeader):0); }
            const uint8_t* data() const 
                { return (fromBlob_?&(*(begin_+sizeof(AudioSampleHeader))):&(*begin_)); }

            static size_t wireLength(size_t payloadLength)
                { return payloadLength+sizeof(AudioSampleHeader); }

        private:
            bool fromBlob_;
            AudioSampleHeader header_;
        };
        
        ENABLE_IF(T,Immutable)
        AudioBundlePacketT(const boost::shared_ptr<const std::vector<uint8_t>>& data):
            HeaderPacketT<CommonHeader,T>(data){}

        ENABLE_IF(T,Mutable)
        AudioBundlePacketT(size_t wireLength):
            HeaderPacketT<CommonHeader,T>(std::vector<uint8_t>()), wireLength_(wireLength)
            { clear(); }

        ENABLE_IF(T,Mutable)
        AudioBundlePacketT(NetworkData&& bundle):
            HeaderPacketT<CommonHeader,T>(boost::move(bundle)){}

        bool hasSpace(const AudioSampleBlob& sampleBlob) const
            { return ((long)remainingSpace_ - (long)DataPacket::wireLength(sampleBlob.size())) >= 0; }

        size_t getRemainingSpace() const { return remainingSpace_; }

        ENABLE_IF(T,Mutable)
        void clear()
        {
            HeaderPacketT<CommonHeader, T>::clear();
            this->remainingSpace_ = AudioBundlePacketT<T>::payloadLength(wireLength_);
        }

        ENABLE_IF(T,Mutable)
        AudioBundlePacketT<T>& operator<<(const AudioSampleBlob& sampleBlob)
        {
            if (hasSpace(sampleBlob))
            {
                this->_data()[0]++;

                uint8_t b1 = sampleBlob.size()&0x00ff, b2 = (sampleBlob.size()&0xff00)>>8; 
                this->payloadBegin_ = this->_data().insert(this->payloadBegin_, b1);
                this->payloadBegin_++;
                this->payloadBegin_ = this->_data().insert(this->payloadBegin_, b2);
                this->payloadBegin_++;
                for (int i = 0; i < sizeof(sampleBlob.getHeader()); ++i)
                {
                    this->payloadBegin_ = this->_data().insert(this->payloadBegin_, ((uint8_t*)&sampleBlob.getHeader())[i]);
                    this->payloadBegin_++;
                }
                // insert blob
                this->_data().insert(this->payloadBegin_, sampleBlob.data(), 
                    sampleBlob.data()+(sampleBlob.size()-sizeof(sampleBlob.getHeader())));
                this->reinit();
                remainingSpace_ -= DataPacket::wireLength(sampleBlob.size());
            }
            else
                throw std::runtime_error("Can not add sample to bundle: no free space");

            return *this;
        }

        size_t getSamplesNum() const 
            { return this->blobs_.size() - this->isHeaderSet(); }
        const AudioSampleBlob operator[](size_t pos) const
            { return AudioSampleBlob(this->blobs_[pos].begin(), this->blobs_[pos].end()); }

        ENABLE_IF(T,Mutable)
        void swap(AudioBundlePacketT<T>& bundle)
        {
            CommonSamplePacket::swap((CommonSamplePacket&)bundle);
            std::swap(wireLength_, bundle.wireLength_);
            std::swap(remainingSpace_, bundle.remainingSpace_);
        }
        
        static size_t wireLength(size_t wireLength, size_t sampleSize)
        {
            size_t sampleWireLength = AudioSampleBlob::wireLength(sampleSize);
            size_t nSamples = AudioBundlePacketT<T>::payloadLength(wireLength)/DataPacket::wireLength(sampleWireLength);
            std::vector<size_t> sampleSizes(nSamples, sampleWireLength);
            sampleSizes.push_back(sizeof(CommonHeader));

            return DataPacket::wireLength(0, sampleSizes);
        }

        static boost::shared_ptr<AudioBundlePacketT<Mutable>> 
            merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>>&);

    private:
        AudioBundlePacketT(AudioBundlePacketT<T>&& bundle) = delete;

        size_t wireLength_, remainingSpace_;

        static size_t payloadLength(size_t wireLength)
        {
            long payloadLength = wireLength-1-DataPacket::wireLength(sizeof(CommonHeader));
            return (payloadLength > 0 ? payloadLength : 0);
        }
    };

    typedef AudioBundlePacketT<Mutable> AudioBundlePacket;
    typedef AudioBundlePacketT<Immutable> ImmutableAudioBundlePacket;

    //******************************************************************************
    /**
     * This is a manifest data packet for (video) samples but can be used for an 
     * arbitrary array of ndn::Data objects.
     * It stores digests of given data objects and provides methods to check whether
     * a data object is a part of this manifest.
     */
    class Manifest : public DataPacket
    {
    public:
         Manifest(const std::vector<boost::shared_ptr<const ndn::Data>>& dataObjects);
         Manifest(NetworkData&& nd);
         
         /**
          * Checks whether given data object is a part of this manifest
          */
         bool hasData(const ndn::Data& data) const;

         /**
          * Returns total number of data objects described by this manifest
          */
         size_t size() const { return blobs_.size(); }
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
        WireSegment(const boost::shared_ptr<ndn::Data>& data, 
            const boost::shared_ptr<const ndn::Interest>& interest);
        WireSegment(const WireSegment& data);

        virtual ~WireSegment(){}

        bool isValid() { return isValid_; }
        size_t getSlicesNum() const;

        boost::shared_ptr<ndn::Data> getData() const { return data_; }
        boost::shared_ptr<const ndn::Interest> getInterest() const { return interest_; }

        ndn::Name getBasePrefix() const { return dataNameInfo_.basePrefix_; }
        unsigned int getApiVersion() const { return dataNameInfo_.apiVersion_; }
        MediaStreamParams::MediaStreamType getStreamType() const { return dataNameInfo_.streamType_; }
        std::string getStreamName() const { return dataNameInfo_.streamName_; }
        bool isMeta() const { return dataNameInfo_.isMeta_; }
        PacketNumber getSampleNo() const { return dataNameInfo_.sampleNo_; }
        bool isDelta() const { return dataNameInfo_.isDelta_; }
        SampleClass getSampleClass() const { return dataNameInfo_.class_; }
        unsigned int getSegNo() const { return dataNameInfo_.segNo_; }
        bool isParity() const { return dataNameInfo_.isParity_; }
        SegmentClass getSegmentClass() const { return dataNameInfo_.segmentClass_; }
        std::string getThreadName() const { return dataNameInfo_.threadName_; }

        bool isPacketHeaderSegment() const { return !dataNameInfo_.isParity_ && dataNameInfo_.segNo_ == 0; }
        virtual PacketNumber getPlaybackNo() const { return getSampleNo(); }
        
        /**
         * This returns a percentage of how much does this segment contributes to
         * the whole packet. For normal data it will be 1/number_of_slices. For 
         * parity data it corresponds to parityWeight().
         * @see fec::parityWeight()
         */
        double getShareSize(unsigned int nDataSlices) const
        { return (dataNameInfo_.isParity_ ? fec::parityWeight()/(double)nDataSlices : 1/(double)nDataSlices); }

        double getSegmentWeight() const
        { return (dataNameInfo_.isParity_ ? fec::parityWeight() : 1); }

        const NamespaceInfo& getInfo() const { return dataNameInfo_; }

        /**
         * Retrieves segment header from data
         * @return DataSegmentHeader
         */
        const DataSegmentHeader header() const;

        /**
         * Retrieves packet header from data only if it's a segment 0 
         * (getSegNo() == 0) because only segment 0 contains packet header.
         * Operation is expensive as it copies data into temporary storage.
         * One may prefer to extract this information after the whole packet
         * was assembled.
         * @return CommonHeader
         */
        const CommonHeader packetHeader() const;

        /**
         * Indicates, whether current packet was retrieved with the interest
         * passed at the construction. This compares nonce value of the interest
         * and nonce value from segment header.
         * @see header()
         */
        bool isOriginal() const;

        static boost::shared_ptr<WireSegment> 
        createSegment(const NamespaceInfo& namespaceInfo,
            const boost::shared_ptr<ndn::Data>& data, 
            const boost::shared_ptr<const ndn::Interest>& interest);

    protected:
        NamespaceInfo dataNameInfo_;
        bool isValid_;
        boost::shared_ptr<ndn::Data> data_;
        boost::shared_ptr<const ndn::Interest> interest_;

        WireSegment(const NamespaceInfo& info,
            const boost::shared_ptr<ndn::Data>& data, 
            const boost::shared_ptr<const ndn::Interest>& interest);
    };

    template<typename SegmentHeader>
    class WireData : public WireSegment 
    {
    public:
        WireData(const boost::shared_ptr<ndn::Data>& data, 
            const boost::shared_ptr<const ndn::Interest>& interest):
            WireSegment(data, interest){}
        WireData(const WireData<SegmentHeader>& data):WireSegment(data){}

        const ImmutableHeaderPacket<SegmentHeader> segment() const
        {
            return ImmutableHeaderPacket<SegmentHeader>(data_->getContent());
        }

        PacketNumber getPlaybackNo() const
        { return playbackNo(); }

    private:
        friend boost::shared_ptr<WireData<SegmentHeader>> 
        boost::make_shared<WireData<SegmentHeader>>(const ndnrtc::NamespaceInfo&,
                                                    const boost::shared_ptr<ndn::Data>&, 
                                                    const boost::shared_ptr<const ndn::Interest>&);

        WireData(const NamespaceInfo& info,
            const boost::shared_ptr<ndn::Data>& data, 
            const boost::shared_ptr<const ndn::Interest>& interest):
            WireSegment(info, data, interest){}

        ENABLE_IF(SegmentHeader,_DataSegmentHeader)
        PacketNumber playbackNo(ENABLE_FOR(_DataSegmentHeader)) const
        { return getSampleNo();  }

        ENABLE_IF(SegmentHeader,_VideoFrameSegmentHeader)
        PacketNumber playbackNo(ENABLE_FOR(_VideoFrameSegmentHeader)) const
        { return segment().getHeader().playbackNo_;  }
    };
};

#endif /* defined(__ndnrtc__frame_slot__) */
