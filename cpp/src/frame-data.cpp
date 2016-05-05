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

#include "ndnrtc-common.h"
#include "frame-data.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

template<>
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
template<>
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

size_t WireSegment::getSlicesNum() const
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

    