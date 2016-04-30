//
//  frame-data.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <cmath>
#include <stdexcept>
#include <ndn-cpp/data.hpp>

#include "ndnrtc-common.h"
#include "frame-data.h"
#include "fec.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
VideoFramePacket::VideoFramePacket(const webrtc::EncodedImage& frame):
CommonSamplePacket(frame._length, frame._buffer), isSyncListSet_(false)
{
    Header hdr;
    hdr.encodedWidth_ = frame._encodedWidth;
    hdr.encodedHeight_ = frame._encodedHeight;
    hdr.timestamp_ = frame._timeStamp;
    hdr.capture_time_ms_ = frame.capture_time_ms_;
    hdr.frameType_ = frame._frameType;
    hdr.completeFrame_ = frame._completeFrame;
    addBlob(sizeof(hdr), (uint8_t*)&hdr);
}

VideoFramePacket::VideoFramePacket(NetworkData&& networkData):
CommonSamplePacket(boost::move(networkData))
{}

const webrtc::EncodedImage& VideoFramePacket::getFrame()
{
    Header *hdr = (Header*)blobs_[0].data();
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, hdr->encodedWidth_, 
        hdr->encodedHeight_);
    frame_ = webrtc::EncodedImage((uint8_t*)(_data().data()+(payloadBegin_-_data().begin())), getPayload().size(), size);
    frame_._encodedWidth = hdr->encodedWidth_;
    frame_._encodedHeight = hdr->encodedHeight_;
    frame_._timeStamp = hdr->timestamp_;
    frame_.capture_time_ms_ = hdr->capture_time_ms_;
    frame_._frameType = hdr->frameType_;
    frame_._completeFrame = hdr->completeFrame_;

    return frame_;
}

boost::shared_ptr<NetworkData>
VideoFramePacket::getParityData(size_t segmentLength, double ratio)
{
    if (!isValid_)
        throw std::runtime_error("Can't compute FEC parity data on invalid packet");

    size_t nDataSegmets = getLength()/segmentLength + (getLength()%segmentLength?1:0);
    size_t nParitySegments = ceil(ratio*nDataSegmets);
    if (nParitySegments == 0) nParitySegments = 1;
    
    std::vector<uint8_t> fecData(nParitySegments*segmentLength, 0);
    fec::Rs28Encoder enc(nDataSegmets, nParitySegments, segmentLength);
    size_t padding =  (nDataSegmets*segmentLength - getLength());
    boost::shared_ptr<NetworkData> parityData;

    // expand data with zeros
    _data().resize(nDataSegmets*segmentLength, 0);
    if (enc.encode(_data().data(), fecData.data()) >= 0)
        parityData = boost::make_shared<NetworkData>(boost::move(fecData));
    // shrink data back
    _data().resize(getLength()-padding);

    return parityData;
}

void
VideoFramePacket::setSyncList(const std::map<std::string, PacketNumber>& syncList)
{
    if (isHeaderSet()) throw std::runtime_error("Can't add more data to this packet"
        " as header has been set already");
    if (isSyncListSet_) throw std::runtime_error("Sync list has been already set");

    for (auto it:syncList)
    {
        addBlob(it.first.size(), (uint8_t*)it.first.c_str());
        addBlob(sizeof(it.second), (uint8_t*)&it.second);
    }

    isSyncListSet_ = true;
}

const std::map<std::string, PacketNumber> 
VideoFramePacket::getSyncList() const
{
    std::map<std::string, PacketNumber> syncList;

    for (std::vector<Blob>::const_iterator blob = blobs_.begin()+1; blob+1 < blobs_.end(); blob+=2)
    {
        syncList[std::string((const char*)blob->data(), blob->size())] = *(PacketNumber*)(blob+1)->data();
    }

    return boost::move(syncList);
}

boost::shared_ptr<VideoFramePacket> 
VideoFramePacket::merge(const std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>>& segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s:segments)
        packetBytes.insert(packetBytes.end(), 
            s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(boost::move(packetBytes));
    return boost::make_shared<VideoFramePacket>(boost::move(packetData));
}

//******************************************************************************
AudioBundlePacket::AudioSampleBlob::
AudioSampleBlob(const std::vector<uint8_t>::const_iterator begin,
               const std::vector<uint8_t>::const_iterator& end):
Blob(begin, end), fromBlob_(true)
{
    header_ = *(AudioSampleHeader*)Blob::data();
}

size_t AudioBundlePacket::AudioSampleBlob::size() const
{ 
    return Blob::size()+(fromBlob_?0:sizeof(AudioSampleHeader)); 
}

size_t AudioBundlePacket::AudioSampleBlob::wireLength(size_t payloadLength)
{
    return payloadLength+sizeof(AudioSampleHeader); 
}

AudioBundlePacket::AudioBundlePacket(size_t wireLength):
CommonSamplePacket(std::vector<uint8_t>()),
wireLength_(wireLength)
{
    clear();
}

AudioBundlePacket::AudioBundlePacket(NetworkData&& data):
CommonSamplePacket(boost::move(data))
{
}

bool 
AudioBundlePacket::hasSpace(const AudioBundlePacket::AudioSampleBlob& sampleBlob) const
{
    return ((long)remainingSpace_ - (long)DataPacket::wireLength(sampleBlob.size())) >= 0;
}

AudioBundlePacket& 
AudioBundlePacket::operator<<(const AudioBundlePacket::AudioSampleBlob& sampleBlob)
{
    if (hasSpace(sampleBlob))
    {
        _data()[0]++;

        uint8_t b1 = sampleBlob.size()&0x00ff, b2 = (sampleBlob.size()&0xff00)>>8; 
        payloadBegin_ = _data().insert(payloadBegin_, b1);
        payloadBegin_++;
        payloadBegin_ = _data().insert(payloadBegin_, b2);
        payloadBegin_++;
        for (int i = 0; i < sizeof(sampleBlob.getHeader()); ++i)
        {
            payloadBegin_ = _data().insert(payloadBegin_, ((uint8_t*)&sampleBlob.getHeader())[i]);
            payloadBegin_++;
        }
        // insert blob
        _data().insert(payloadBegin_, sampleBlob.data(), 
            sampleBlob.data()+(sampleBlob.size()-sizeof(sampleBlob.getHeader())));
        reinit();
        remainingSpace_ -= DataPacket::wireLength(sampleBlob.size());
    }

    return *this;
}

void AudioBundlePacket::clear()
{
    _data().clear();
    _data().insert(_data().begin(),0);
    payloadBegin_ = _data().begin()+1;
    blobs_.clear();
    remainingSpace_ = AudioBundlePacket::payloadLength(wireLength_);
}

size_t AudioBundlePacket::getSamplesNum() const
{
    return blobs_.size() - isHeaderSet();
}

void AudioBundlePacket::swap(AudioBundlePacket& bundle)
{ 
    CommonSamplePacket::swap((CommonSamplePacket&)bundle);
    std::swap(wireLength_, bundle.wireLength_);
    std::swap(remainingSpace_, bundle.remainingSpace_);
}

const AudioBundlePacket::AudioSampleBlob 
AudioBundlePacket::operator[](size_t pos) const
{
    return AudioSampleBlob(blobs_[pos].begin(), blobs_[pos].end());
}

size_t AudioBundlePacket::wireLength(size_t wireLength, size_t sampleSize)
{
    size_t sampleWireLength = AudioSampleBlob::wireLength(sampleSize);
    size_t nSamples = AudioBundlePacket::payloadLength(wireLength)/DataPacket::wireLength(sampleWireLength);
    std::vector<size_t> sampleSizes(nSamples, sampleWireLength);
    sampleSizes.push_back(sizeof(CommonHeader));

    return DataPacket::wireLength(0, sampleSizes);
}

size_t AudioBundlePacket::payloadLength(size_t wireLength)
{
    long payloadLength = wireLength-1-DataPacket::wireLength(sizeof(CommonHeader));
    return (payloadLength > 0 ? payloadLength : 0);
}

boost::shared_ptr<AudioBundlePacket> 
AudioBundlePacket::merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>>& segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s:segments)
        packetBytes.insert(packetBytes.end(), 
            s.getPayload().begin(), s.getPayload().end());
    
    NetworkData packetData(boost::move(packetBytes));
    return boost::make_shared<AudioBundlePacket>(boost::move(packetData));
}

//******************************************************************************
AudioThreadMeta::AudioThreadMeta(double rate, const std::string& codec):
DataPacket(std::vector<uint8_t>())
{
    addBlob(sizeof(rate), (uint8_t*)&rate);
    if (codec.size()) addBlob(codec.size(), (uint8_t*)codec.c_str());
    else isValid_ = false;
}

AudioThreadMeta::AudioThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 2 && blobs_[0].size() == sizeof(double));
}

double 
AudioThreadMeta::getRate() const
{
    return *(const double*)blobs_[0].data();
}

std::string
AudioThreadMeta::getCodec() const
{
    return std::string((const char*)blobs_[1].data(), blobs_[1].size());
}

//******************************************************************************
VideoThreadMeta::VideoThreadMeta(double rate, const FrameSegmentsInfo& segInfo,
    const VideoCoderParams& coder):
DataPacket(std::vector<uint8_t>())
{
    Meta m({rate, coder.gop_, coder.startBitrate_, coder.encodeWidth_, coder.encodeHeight_, 
        segInfo.deltaAvgSegNum_, segInfo.deltaAvgParitySegNum_,
        segInfo.keyAvgSegNum_, segInfo.keyAvgParitySegNum_});
    addBlob(sizeof(m), (uint8_t*)&m);
}

VideoThreadMeta::VideoThreadMeta(NetworkData&& data):
DataPacket(boost::move(data))
{
    isValid_ = (blobs_.size() == 1 && blobs_[0].size() == sizeof(Meta));
}

double VideoThreadMeta::getRate() const
{
    return ((Meta*)blobs_[0].data())->rate_;
}

FrameSegmentsInfo VideoThreadMeta::getSegInfo() const
{
    Meta *m = (Meta*)blobs_[0].data();
    return FrameSegmentsInfo({m->deltaAvgSegNum_, m->deltaAvgParitySegNum_, 
        m->keyAvgSegNum_, m->keyAvgParitySegNum_});
}

VideoCoderParams VideoThreadMeta::getCoderParams() const
{
    Meta *m = (Meta*)blobs_[0].data();
    VideoCoderParams c;
    c.gop_ = m->gop_;
    c.startBitrate_ = m->bitrate_;
    c.encodeWidth_ = m->width_;
    c.encodeHeight_ = m->height_;
    return c;
}

//******************************************************************************
#define SYNC_MARKER "sync:"
MediaStreamMeta::MediaStreamMeta():
DataPacket(std::vector<uint8_t>())
{}

MediaStreamMeta::MediaStreamMeta(std::vector<std::string> threads):
DataPacket(std::vector<uint8_t>())
{
    for (auto t:threads) addThread(t);
}

void
MediaStreamMeta::addThread(const std::string& thread)
{
     addBlob(thread.size(), (uint8_t*)thread.c_str());
}

void 
MediaStreamMeta::addSyncStream(const std::string& stream)
{
    addThread("sync:"+stream);
}

std::vector<std::string>
MediaStreamMeta::getSyncStreams() const
{
    std::vector<std::string> syncStreams;
    for (auto b:blobs_)
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        size_t p = thread.find(SYNC_MARKER);
        if (p != std::string::npos)
            syncStreams.push_back(std::string(thread.begin()+sizeof(SYNC_MARKER)-1, 
                thread.end()));
            
    }
    return syncStreams;
}

std::vector<std::string>
MediaStreamMeta::getThreads() const 
{
    std::vector<std::string> threads;
    for (auto b:blobs_) 
    {
        std::string thread = std::string((const char*)b.data(), b.size());
        if (thread.find("sync:") == std::string::npos)
            threads.push_back(thread);
    }
    return threads;
}

//******************************************************************************
WireSegment::WireSegment(const boost::shared_ptr<ndn::Data>& data):data_(data),
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

WireSegment::WireSegment(const WireSegment& data):data_(data.data_),
dataNameInfo_(data.dataNameInfo_), isValid_(data.isValid_){}

size_t WireSegment::getSlicesNum()
{
    return data_->getMetaInfo().getFinalBlockId().toNumber();
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
    if (getSegNo()) throw std::runtime_error("Accessing packet header in "
        "non-zero segment is not allowed");

        ImmutableHeaderPacket<DataSegmentHeader> s0(data_->getContent());
    boost::shared_ptr<std::vector<uint8_t>> data(boost::make_shared<std::vector<uint8_t>>(s0.getPayload().begin(), 
        s0.getPayload().end()));
    ImmutableHeaderPacket<CommonHeader> p0(data);
    return p0.getHeader();
}

