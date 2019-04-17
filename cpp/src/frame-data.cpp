//
//  frame-data.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <cmath>
#include <stdexcept>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>

#include "ndnrtc-common.hpp"
#include "frame-data.hpp"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

namespace ndnrtc
{
template <>
std::shared_ptr<VideoFramePacket>
VideoFramePacket::merge(const std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> &segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s : segments)
        packetBytes.insert(packetBytes.end(),
                           s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(boost::move(packetBytes));
    return std::make_shared<VideoFramePacket>(boost::move(packetData));
}

//******************************************************************************
template <>
std::shared_ptr<AudioBundlePacket>
AudioBundlePacket::merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>> &segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s : segments)
        packetBytes.insert(packetBytes.end(),
                           s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(boost::move(packetBytes));
    return std::make_shared<AudioBundlePacket>(boost::move(packetData));
}
}
//******************************************************************************
Manifest::Manifest(const std::vector<std::shared_ptr<const ndn::Data>> &dataObjects)
    : DataPacket(std::vector<uint8_t>())
{
    for (auto &d : dataObjects)
    {
        ndn::Blob digest = (*d->getFullName())[-1].getValue();
        addBlob(digest.size(), digest.buf());
    }
}

Manifest::Manifest(NetworkData &&nd) : DataPacket(boost::move(nd)) {}

bool Manifest::hasData(const ndn::Data &data) const
{
    ndn::Blob digest = (*data.getFullName())[-1].getValue();

    for (int i = 0; i < getBlobsNum(); ++i)
    {
        ndn::Blob b(getBlob(i).data(), getBlob(i).size());
        if (b.equals(digest))
            return true;
    }
    return false;
}

//******************************************************************************
AudioThreadMeta::AudioThreadMeta(double rate, uint64_t bundleNo, const std::string &codec)
    : DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(rate), (uint8_t *)&rate);
    addBlob(sizeof(bundleNo), (uint8_t *)&bundleNo);
    if (codec.size())
        addBlob(codec.size(), (uint8_t *)codec.c_str());
    else
        isValid_ = false;
}

AudioThreadMeta::AudioThreadMeta(NetworkData &&data) : DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 3 &&
                blobs_[0].size() == sizeof(double) &&
                blobs_[1].size() == sizeof(uint64_t));
}

double
AudioThreadMeta::getRate() const
{
    return *(const double *)blobs_[0].data();
}

uint64_t
AudioThreadMeta::getBundleNo() const
{
    return *(const uint64_t *)blobs_[1].data();
}

std::string
AudioThreadMeta::getCodec() const
{
    return std::string((const char *)blobs_[2].data(), blobs_[2].size());
}

//******************************************************************************
VideoThreadMeta::VideoThreadMeta(double rate, PacketNumber deltaSeqNo, PacketNumber keySeqNo,
                                 unsigned char gopPos, const FrameSegmentsInfo &segInfo, const VideoCoderParams &coder)
    : DataPacket(std::vector<uint8_t>())
{
    Meta m({rate, deltaSeqNo, keySeqNo, gopPos,
            coder.gop_, coder.startBitrate_, coder.encodeWidth_, coder.encodeHeight_,
            segInfo.deltaAvgSegNum_, segInfo.deltaAvgParitySegNum_,
            segInfo.keyAvgSegNum_, segInfo.keyAvgParitySegNum_});
    addBlob(sizeof(m), (uint8_t *)&m);
}

VideoThreadMeta::VideoThreadMeta(NetworkData &&data) : DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 1 && blobs_[0].size() == sizeof(Meta));
}

double VideoThreadMeta::getRate() const
{
    return ((Meta *)blobs_[0].data())->rate_;
}

pair<PacketNumber, PacketNumber> VideoThreadMeta::getSeqNo() const
{
    pair<PacketNumber, PacketNumber> seqNo;
    seqNo.first = ((Meta *)blobs_[0].data())->deltaSeqNo_;
    seqNo.second = ((Meta *)blobs_[0].data())->keySeqNo_;
    return seqNo;
}

unsigned char VideoThreadMeta::getGopPos() const
{
    return ((Meta *)blobs_[0].data())->gopPos_;
}

FrameSegmentsInfo VideoThreadMeta::getSegInfo() const
{
    Meta *m = (Meta *)blobs_[0].data();
    return FrameSegmentsInfo({m->deltaAvgSegNum_, m->deltaAvgParitySegNum_,
                              m->keyAvgSegNum_, m->keyAvgParitySegNum_});
}

VideoCoderParams VideoThreadMeta::getCoderParams() const
{
    Meta *m = (Meta *)blobs_[0].data();
    VideoCoderParams c;
    c.gop_ = m->gop_;
    c.startBitrate_ = m->bitrate_;
    c.encodeWidth_ = m->width_;
    c.encodeHeight_ = m->height_;
    return c;
}

//******************************************************************************
#define SYNC_MARKER "sync:"
MediaStreamMeta::MediaStreamMeta(uint64_t timestamp) : DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(uint64_t), (uint8_t*)&timestamp); // blob 0 is the timestamp
}

MediaStreamMeta::MediaStreamMeta(uint64_t timestamp, std::vector<std::string> threads) : DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(uint64_t), (uint8_t*)&timestamp); // blob 0 is the timestamp

    for (auto t : threads)
        addThread(t);
}

void MediaStreamMeta::addThread(const std::string &thread)
{
    addBlob(thread.size(), (uint8_t *)thread.c_str());
}

void MediaStreamMeta::addSyncStream(const std::string &stream)
{
    addThread(SYNC_MARKER + stream);
}

std::vector<std::string>
MediaStreamMeta::getSyncStreams() const
{
    std::vector<std::string> syncStreams;
    for (auto b : blobs_)
    {
        std::string thread = std::string((const char *)b.data(), b.size());
        size_t p = thread.find(SYNC_MARKER);
        if (p != std::string::npos)
            syncStreams.push_back(std::string(thread.begin() + sizeof(SYNC_MARKER) - 1,
                                              thread.end()));
    }
    return syncStreams;
}

std::vector<std::string>
MediaStreamMeta::getThreads() const
{
    bool skipFirst = true; // first is a timestamp
    std::vector<std::string> threads;
    for (auto b : blobs_)
    {
        if (skipFirst)
        {
            skipFirst = false;
            continue;
        }

        std::string thread = std::string((const char *)b.data(), b.size());
        if (thread.find("sync:") == std::string::npos)
            threads.push_back(thread);
    }
    return threads;
}

uint64_t
MediaStreamMeta::getStreamTimestamp() const
{
    return *(uint64_t*)getBlob(0).data();
}

//******************************************************************************
WireSegment::WireSegment(const std::shared_ptr<ndn::Data> &data,
                         const std::shared_ptr<const ndn::Interest> &interest)
    : data_(data), interest_(interest),
      isValid_(NameComponents::extractInfo(data->getName(), dataNameInfo_))
{
    if (dataNameInfo_.apiVersion_ != NameComponents::nameApiVersion())
    {
        std::stringstream ss;
        ss << "Attempt to create wired data object with "
           << "unsupported namespace API version: " << dataNameInfo_.apiVersion_ 
           << " (current version is " << NameComponents::nameApiVersion() << ")"
           << std::endl;
        throw std::runtime_error(ss.str());
    }
}

WireSegment::WireSegment(const NamespaceInfo &info,
                         const std::shared_ptr<ndn::Data> &data,
                         const std::shared_ptr<const ndn::Interest> &interest)
    : dataNameInfo_(info), data_(data), interest_(interest), isValid_(true)
{
    if (dataNameInfo_.apiVersion_ != NameComponents::nameApiVersion())
    {
        std::stringstream ss;
        ss << "Attempt to create wired data object with "
           << "unsupported namespace API version: " << dataNameInfo_.apiVersion_ << std::endl;
        throw std::runtime_error(ss.str());
    }
}

WireSegment::WireSegment(const WireSegment &data) : data_(data.data_),
                                                    dataNameInfo_(data.dataNameInfo_), isValid_(data.isValid_) {}

size_t WireSegment::getSlicesNum() const
{
    return data_->getMetaInfo().getFinalBlockId().toSegment() + 1;
}

const DataSegmentHeader
WireSegment::header() const
{
    // this cast will be invalid for VideoFrameSegment packets,
    // still casting to DataSegmentHeader is possible, because
    // it's a parent class for VideoFrameSegmentHeader
    ImmutableHeaderPacket<DataSegmentHeader> s(data_->getContent());
    return s.getHeader();
}

const CommonHeader
WireSegment::packetHeader() const
{
    if (getSegNo())
        throw std::runtime_error("Accessing packet header in "
                                 "non-zero segment is not allowed");

    ImmutableHeaderPacket<DataSegmentHeader> s0(data_->getContent());
    std::shared_ptr<std::vector<uint8_t>> data(std::make_shared<std::vector<uint8_t>>(s0.getPayload().begin(),
                                                                                          s0.getPayload().end()));
    ImmutableHeaderPacket<CommonHeader> p0(data);
    return p0.getHeader();
}

bool WireSegment::isOriginal() const
{
    if (!interest_->getNonce().size())
        return false; //throw std::runtime_error("Interest nonce is not set");

    return header().interestNonce_ == *(uint32_t *)(interest_->getNonce().buf());
}

std::shared_ptr<WireSegment>
WireSegment::createSegment(const NamespaceInfo &namespaceInfo,
                           const std::shared_ptr<ndn::Data> &data,
                           const std::shared_ptr<const ndn::Interest> &interest)
{
    if (namespaceInfo.streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo &&
        (namespaceInfo.segmentClass_ == SegmentClass::Data || namespaceInfo.segmentClass_ == SegmentClass::Parity))
        return std::make_shared<WireData<VideoFrameSegmentHeader>>(namespaceInfo, data, interest);

    return std::make_shared<WireData<DataSegmentHeader>>(namespaceInfo, data, interest);
    ;
}
