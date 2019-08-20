//
// network-data.cpp
//
//  Created by Peter Gusev on 11 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "network-data.hpp"

#include <bitset>

#include <ndn-cpp/c/common.h>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/interest.hpp>

#include "fec.hpp"
#include "clock.hpp"
#include "packets.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;

size_t SegmentsManifest::DigestSize = ndn_SHA256_DIGEST_SIZE;

namespace ndnrtc {

ostream& operator<<(ostream& out, const DataRequest::Status v)
{
    static map<DataRequest::Status, string> Labels = {
        {DataRequest::Status::Created, "Created"},
        {DataRequest::Status::Expressed, "Expressed"},
        {DataRequest::Status::Timeout, "Timeout"},
        {DataRequest::Status::AppNack, "AppNack"},
        {DataRequest::Status::NetworkNack, "NetworkNack"},
        {DataRequest::Status::Data, "Data"},
    };

    out << Labels[v];
    return out;
}

vector<DataRequest::Status> fromMask(uint8_t statusMask)
{
    vector<DataRequest::Status> statuses = {
        DataRequest::Status::Created,
        DataRequest::Status::Expressed,
        DataRequest::Status::Timeout,
        DataRequest::Status::AppNack,
        DataRequest::Status::NetworkNack,
        DataRequest::Status::Data
    };
    vector<DataRequest::Status>::iterator it = statuses.begin();

    while (it != statuses.end())
    {
        if (! (*it & statusMask))
            statuses.erase(it);
        else
            ++it;
    }

    return statuses;
}

uint8_t toMask(DataRequest::Status s)
{
    return static_cast<uint8_t>(s);
}

}

std::shared_ptr<ndn::Data>
SegmentsManifest::packManifest(const Name& n, const vector<std::shared_ptr<Data>>& segments)
{
    std::shared_ptr<Data> manifest = std::make_shared<Data>(n);

    vector<uint8_t> payload(DigestSize*segments.size(), 0);
    uint8_t *ptr = payload.data();

    for (auto &d : segments)
    {
        Blob digest = (*d->getFullName())[-1].getValue();
        assert(digest.size() == DigestSize);

        copy(digest->begin(), digest->end(), ptr);
        ptr += DigestSize;
    }

    manifest->setContent(payload);
    return manifest;
}

bool
SegmentsManifest::hasData(const Data& manifest, const Data& d)
{
    Blob digest = (*d.getFullName())[-1].getValue();
    const uint8_t *ptr = manifest.getContent().buf();

    while (ptr - manifest.getContent().buf() < manifest.getContent().size())
    {
        Blob b(ptr, digest.size());
        if (b.equals(digest))
            return true;
        ptr += digest.size();
    }

    return false;
}

DataRequest::DataRequest(const std::shared_ptr<const Interest>& interest)
: interest_(interest)
, requestTsUsec_(0)
, replyTsUsec_(0)
, rtxNum_(0)
, timeoutNum_(0)
, netwNackNum_(0)
, appNackNum_(0)
, status_(Status::Created)
, id_(0)
{
    if (!NameComponents::extractInfo(interest->getName(), namespaceInfo_))
        throw runtime_error("error creating DataRequest: failed to extract"
                            " NamespaceInfo from interest name");
}

RequestTriggerConnection
DataRequest::subscribe(Status s, OnRequestUpdate onRequestUpdate)
{
    RequestTriggerConnection c;

    switch (s) {
        case Status::Expressed: c = onExpressed_.connect(onRequestUpdate); break;
        case Status::Timeout: c = onTimeout_.connect(onRequestUpdate); break;
        case Status::AppNack: c = onAppNack_.connect(onRequestUpdate); break;
        case Status::NetworkNack: c = onNetworkNack_.connect(onRequestUpdate); break;
        case Status::Data: c = onData_.connect(onRequestUpdate); break;
        default: break;
    }

    return c;
}

void
DataRequest::triggerEvent(Status s)
{
    switch (s) {
        case Status::Expressed: {
            onExpressed_(*this);
        } break;

        case Status::Timeout: {
            onTimeout_(*this);
        } break;

        case Status::NetworkNack: {
            onNetworkNack_(*this);
        } break;

        case Status::AppNack : {
            onAppNack_(*this);
        } break;

        case Status::Data : {
            onData_(*this);
        } break;

        default : break;
    }
}

void
DataRequest::timestampRequest()
{
    requestTsUsec_ = clock::microsecondTimestamp();
    replyTsUsec_ = 0;
}

void
DataRequest::timestampReply()
{
    replyTsUsec_ = clock::microsecondTimestamp();
}

void
DataRequest::setData(const std::shared_ptr<const Data>& data)
{
    if (!interest_->getName().isPrefixOf(data->getName()))
        throw runtime_error("DataRequest::setData: interest name is not a "
                            "prefix of data name");

    data_ = data;
    NameComponents::extractInfo(data->getName(), namespaceInfo_);

    if (data->getMetaInfo().getType() == ndn_ContentType_NACK)
        appNackNum_++;
    else
        packet_ = packets::BasePacket::ndnrtcPacketFromRequest(*this);
}

void
DataRequest::setNack(const std::shared_ptr<const ndn::NetworkNack> &nack)
{
    netwNackNum_++;
    nack_ = nack;
}

void
DataRequest::setTimeout()
{
    timeoutNum_++;
}

void
DataRequest::invokeWhenAll(vector<std::shared_ptr<DataRequest> > requests,
                           DataRequest::Status status,
                           OnRequestsReady onRequestsReady)
{
    // up to 64 requests is supported
    uint64_t unusedBitMask = (~0) << requests.size();
    std::shared_ptr<bitset<64>> rBitset = std::make_shared<bitset<64>>(unusedBitMask);
    auto onStatusUpdate = [rBitset, requests, onRequestsReady](const DataRequest& r){
        bool gotResult = false;
        int rPos = 0;

        for (auto &dr:requests)
        {
            if ((gotResult = (dr.get() == &r)))
                break;
            rPos++;
        }
        assert(gotResult);
        rBitset->set(rPos);

        // check if all requests are ready
        if (rBitset->all())
            onRequestsReady(requests);
    };

    for (auto &r:requests)
        r->subscribe(status, onStatusUpdate);
}

void
DataRequest::invokeIfAny(std::vector<std::shared_ptr<DataRequest> > requests,
                         uint8_t statusMask,
                         OnRequestsReady onRequestsReady)
{
    // up to 64 requests is supported
    std::shared_ptr<bitset<64>> rBitset = std::make_shared<bitset<64>>(0);
    vector<DataRequest::Status> statuses = fromMask(statusMask);
    auto completed = std::make_shared<vector<std::shared_ptr<DataRequest>>>();
    auto onStatusUpdate = [rBitset, requests, onRequestsReady, completed](const DataRequest& r){
        bool gotResult = false;
        int rPos = 0;

        for (auto &dr:requests)
        {
            if ((gotResult = (dr.get() == &r)))
                break;
            rPos++;
        }
        assert(gotResult);
        rBitset->set(rPos);

        if (rBitset->any())
        {
            // invoke with array of completed requests
            completed->push_back(requests[rPos]);
            onRequestsReady(*completed);
        }
    };

    for (auto &s:statuses)
        for (auto &r:requests)
            r->subscribe(s, onStatusUpdate);
}

//******************************************************************************
// CODE BELOW IS DEPRECATED
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

Manifest::Manifest(NetworkData &&nd) : DataPacket(std::move(nd)) {}

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

AudioThreadMeta::AudioThreadMeta(NetworkData &&data) : DataPacket(std::move(data))
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

VideoThreadMeta::VideoThreadMeta(NetworkData &&data) : DataPacket(std::move(data))
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

double
WireSegment::getShareSize(unsigned int nDataSlices) const
{
    return (dataNameInfo_.isParity_ ? fec::parityWeight() / (double)nDataSlices : 1 / (double)nDataSlices);
}

double
WireSegment::getSegmentWeight() const
{
    return (dataNameInfo_.isParity_ ? fec::parityWeight() : 1);
}

std::shared_ptr<WireSegment>
WireSegment::createSegment(const NamespaceInfo &namespaceInfo,
                           const std::shared_ptr<ndn::Data> &data,
                           const std::shared_ptr<const ndn::Interest> &interest)
{
    if (namespaceInfo.streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo &&
        (namespaceInfo.segmentClass_ == SegmentClass::Data || namespaceInfo.segmentClass_ == SegmentClass::Parity))
        return std::shared_ptr<WireData<VideoFrameSegmentHeader>>(new WireData<VideoFrameSegmentHeader>(namespaceInfo, data, interest));

    return std::shared_ptr<WireData<DataSegmentHeader>>(new WireData<DataSegmentHeader>(namespaceInfo, data, interest));
    ;
}
