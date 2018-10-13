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

#include "network-data.hpp"
#include "webrtc.hpp"
#include "name-components.hpp"
#include "fec.hpp"

namespace ndnrtc 
{
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
class DataSegment : protected DataPacket::Blob
{
  public:
    DataSegment(const DataSegment &s) : Blob(s), header_(s.header_) {}
    DataSegment(const std::vector<uint8_t>::const_iterator &begin,
                const std::vector<uint8_t>::const_iterator &end) : Blob(begin, end)
    {
        memset(&header_, 0, sizeof(header_));
    }

    void setHeader(const DataSegmentHeader &header) { header_ = reinterpret_cast<const Header &>(header); }
    const Header &getHeader() const { return header_; }
    const DataPacket::Blob &getPayload() const { return *this; }
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
        long payloadLength = wireLength - 1 - DataPacket::wireLength(sizeof(Header));
        return (payloadLength < 0) ? 0 : payloadLength;
    }

    static size_t numSlices(const NetworkData &nd,
                            size_t segmentWireLength)
    {
        size_t payloadLength = DataSegment<Header>::payloadLength(segmentWireLength);
        return (nd.getLength() / payloadLength) + (nd.getLength() % payloadLength ? 1 : 0);
    }

    static std::vector<DataSegment<Header>> slice(const NetworkData &nd,
                                                  size_t segmentWireLength)
    {
        std::vector<DataSegment<Header>> segments;
        size_t payloadLength = DataSegment<Header>::payloadLength(segmentWireLength);

        if (payloadLength == 0)
            return segments;

        std::vector<uint8_t>::const_iterator p1 = nd.data().begin();
        std::vector<uint8_t>::const_iterator p2 = p1 + payloadLength;

        while (p2 < nd.data().end())
        {
            segments.push_back(DataSegment<Header>(p1, p2));
            p1 = p2;
            p2 += payloadLength;
        }

        segments.push_back(DataSegment<Header>(p1, nd.data().end()));

        return segments;
    }

    static size_t headerSize() { return sizeof(Header); }

  private:
    Header header_;
};

typedef DataSegment<VideoFrameSegmentHeader> VideoFrameSegment;
typedef DataSegment<DataSegmentHeader> CommonSegment;

/*******************************************************************************
 * VideoFramePacket provides interface for preparing encoded video frame for
 * publishing as a data packet.
 */
template <typename T = Mutable>
class VideoFramePacketT : public HeaderPacketT<CommonHeader, T>
{
  public:
    typedef std::map<std::string, PacketNumber> ThreadSyncList;

    ENABLE_IF(T, Immutable)
    VideoFramePacketT(const boost::shared_ptr<const std::vector<uint8_t>> &data) : HeaderPacketT<CommonHeader, T>(data) {}

    ENABLE_IF(T, Mutable)
    VideoFramePacketT(const webrtc::EncodedImage &frame) : HeaderPacketT<CommonHeader, T>(frame._length, frame._buffer),
                                                           isSyncListSet_(false)
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
        this->addBlob(sizeof(hdr), (uint8_t *)&hdr);
    }

    ENABLE_IF(T, Mutable)
    VideoFramePacketT(NetworkData &&networkData) : CommonSamplePacket(boost::move(networkData)) {}

    const webrtc::EncodedImage &getFrame()
    {
        Header *hdr = (Header *)this->blobs_[0].data();
        int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_,
                                              hdr->encodedHeight_);
        frame_ = webrtc::EncodedImage((uint8_t *)(this->_data().data() + (this->payloadBegin_ - this->_data().begin())),
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

        for (BlobIterator blob = this->blobs_.begin() + 1;
             blob + 1 < this->blobs_.end(); blob += 2)
            syncList[std::string((const char *)blob->data(), blob->size())] = *(PacketNumber *)(blob + 1)->data();

        return boost::move(syncList);
    }

    ENABLE_IF(T, Mutable)
    boost::shared_ptr<NetworkData>
    getParityData(size_t segmentLength, double ratio)
    {
        if (!this->isValid_)
            throw std::runtime_error("Can't compute FEC parity data on invalid packet");

        size_t nDataSegmets = this->getLength() / segmentLength + (this->getLength() % segmentLength ? 1 : 0);
        size_t nParitySegments = ceil(ratio * nDataSegmets);
        if (nParitySegments == 0)
            nParitySegments = 1;

        std::vector<uint8_t> fecData(nParitySegments * segmentLength, 0);
        fec::Rs28Encoder enc(nDataSegmets, nParitySegments, segmentLength);
        size_t padding = (nDataSegmets * segmentLength - this->getLength());
        boost::shared_ptr<NetworkData> parityData;

        // expand data with zeros
        this->_data().resize(nDataSegmets * segmentLength, 0);
        if (enc.encode(this->_data().data(), fecData.data()) >= 0)
            parityData = boost::make_shared<NetworkData>(boost::move(fecData));
        // shrink data back
        this->_data().resize(this->getLength() - padding);
        this->reinit(); // data may have been relocated, so we need to reinit blobs

        return parityData;
    }

    ENABLE_IF(T, Mutable)
    void setSyncList(const ThreadSyncList &syncList)
    {
        if (this->isHeaderSet())
            throw std::runtime_error("Can't add more data to this packet"
                                     " as header has been set already");
        if (isSyncListSet_)
            throw std::runtime_error("Sync list has been already set");

        for (auto it : syncList)
        {
            this->addBlob(it.first.size(), (uint8_t *)it.first.c_str());
            this->addBlob(sizeof(it.second), (uint8_t *)&it.second);
        }

        isSyncListSet_ = true;
    }

    typedef std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> ImmutableVideoSegmentsVector;
    typedef std::vector<ImmutableHeaderPacket<DataSegmentHeader>> ImmutableRecoverySegmentsVector;

    static boost::shared_ptr<VideoFramePacketT<>>
    merge(const ImmutableVideoSegmentsVector &segments);

  private:
    typedef struct _Header
    {
        uint32_t encodedWidth_;
        uint32_t encodedHeight_;
        uint32_t timestamp_;
        int64_t capture_time_ms_;
        WebRtcVideoFrameType frameType_;
        bool completeFrame_;
        uint32_t frameLength_;
    } __attribute__((packed)) Header;

    webrtc::EncodedImage frame_;
    bool isSyncListSet_;
};

typedef VideoFramePacketT<> VideoFramePacket;
typedef VideoFramePacketT<Immutable> ImmutableVideoFramePacket;

/*******************************************************************************
 * AudioBundlePacket provides interface for bundling audio samples and 
 * preparing them for publishing as a data packet.
 */
template <typename T>
class AudioBundlePacketT : public HeaderPacketT<CommonHeader, T>
{
  public:
    class AudioSampleBlob : protected DataPacket::Blob
    {
      public:
        AudioSampleBlob(const AudioSampleHeader &hdr, const std::vector<uint8_t>::const_iterator begin,
                        const std::vector<uint8_t>::const_iterator &end) : Blob(begin, end), header_(hdr), fromBlob_(false) {}

        AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
                        const std::vector<uint8_t>::const_iterator &end) : Blob(begin, end), fromBlob_(true)
        {
            header_ = *(AudioSampleHeader *)Blob::data();
        }

        const AudioSampleHeader &getHeader() const
        {
            return header_;
        }
        size_t size() const
        {
            return Blob::size() + (fromBlob_ ? 0 : sizeof(AudioSampleHeader));
        }
        size_t payloadLength() const
        {
            return Blob::size() + (fromBlob_ ? -sizeof(AudioSampleHeader) : 0);
        }
        const uint8_t *data() const
        {
            return (fromBlob_ ? &(*(begin_ + sizeof(AudioSampleHeader))) : &(*begin_));
        }

        static size_t wireLength(size_t payloadLength)
        {
            return payloadLength + sizeof(AudioSampleHeader);
        }

      private:
        bool fromBlob_;
        AudioSampleHeader header_;
    };

    ENABLE_IF(T, Immutable)
    AudioBundlePacketT(const boost::shared_ptr<const std::vector<uint8_t>> &data) : HeaderPacketT<CommonHeader, T>(data) {}

    ENABLE_IF(T, Mutable)
    AudioBundlePacketT(size_t wireLength) : HeaderPacketT<CommonHeader, T>(std::vector<uint8_t>()), wireLength_(wireLength)
    {
        clear();
    }

    ENABLE_IF(T, Mutable)
    AudioBundlePacketT(NetworkData &&bundle) : HeaderPacketT<CommonHeader, T>(boost::move(bundle)) {}

    bool hasSpace(const AudioSampleBlob &sampleBlob) const
    {
        return ((long)remainingSpace_ - (long)DataPacket::wireLength(sampleBlob.size())) >= 0;
    }

    size_t getRemainingSpace() const { return remainingSpace_; }

    ENABLE_IF(T, Mutable)
    void clear()
    {
        HeaderPacketT<CommonHeader, T>::clear();
        this->remainingSpace_ = AudioBundlePacketT<T>::payloadLength(wireLength_);
    }

    ENABLE_IF(T, Mutable)
    AudioBundlePacketT<T> &operator<<(const AudioSampleBlob &sampleBlob)
    {
        if (hasSpace(sampleBlob))
        {
            this->_data()[0]++;

            uint8_t b1 = sampleBlob.size() & 0x00ff, b2 = (sampleBlob.size() & 0xff00) >> 8;
            this->payloadBegin_ = this->_data().insert(this->payloadBegin_, b1);
            this->payloadBegin_++;
            this->payloadBegin_ = this->_data().insert(this->payloadBegin_, b2);
            this->payloadBegin_++;
            for (int i = 0; i < sizeof(sampleBlob.getHeader()); ++i)
            {
                this->payloadBegin_ = this->_data().insert(this->payloadBegin_, ((uint8_t *)&sampleBlob.getHeader())[i]);
                this->payloadBegin_++;
            }
            // insert blob
            this->_data().insert(this->payloadBegin_, sampleBlob.data(),
                                 sampleBlob.data() + (sampleBlob.size() - sizeof(sampleBlob.getHeader())));
            this->reinit();
            remainingSpace_ -= DataPacket::wireLength(sampleBlob.size());
        }
        else
            throw std::runtime_error("Can not add sample to bundle: no free space");

        return *this;
    }

    size_t getSamplesNum() const
    {
        return this->blobs_.size() - this->isHeaderSet();
    }
    const AudioSampleBlob operator[](size_t pos) const
    {
        return AudioSampleBlob(this->blobs_[pos].begin(), this->blobs_[pos].end());
    }

    ENABLE_IF(T, Mutable)
    void swap(AudioBundlePacketT<T> &bundle)
    {
        CommonSamplePacket::swap((CommonSamplePacket &)bundle);
        std::swap(wireLength_, bundle.wireLength_);
        std::swap(remainingSpace_, bundle.remainingSpace_);
    }

    static size_t wireLength(size_t wireLength, size_t sampleSize)
    {
        size_t sampleWireLength = AudioSampleBlob::wireLength(sampleSize);
        size_t nSamples = AudioBundlePacketT<T>::payloadLength(wireLength) / DataPacket::wireLength(sampleWireLength);
        std::vector<size_t> sampleSizes(nSamples, sampleWireLength);
        sampleSizes.push_back(sizeof(CommonHeader));

        return DataPacket::wireLength(0, sampleSizes);
    }

    static boost::shared_ptr<AudioBundlePacketT<Mutable>>
    merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>> &);

  private:
    AudioBundlePacketT(AudioBundlePacketT<T> &&bundle) = delete;

    size_t wireLength_, remainingSpace_;

    static size_t payloadLength(size_t wireLength)
    {
        long payloadLength = wireLength - 1 - DataPacket::wireLength(sizeof(CommonHeader));
        return (payloadLength > 0 ? payloadLength : 0);
    }
};

typedef AudioBundlePacketT<Mutable> AudioBundlePacket;
typedef AudioBundlePacketT<Immutable> ImmutableAudioBundlePacket;

}

#endif /* defined(__ndnrtc__frame_slot__) */
