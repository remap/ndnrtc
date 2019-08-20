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

namespace ndnrtc {

template <>
std::shared_ptr<VideoFramePacket>
VideoFramePacket::merge(const std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> &segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s : segments)
        packetBytes.insert(packetBytes.end(),
                           s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(std::move(packetBytes));
    return std::make_shared<VideoFramePacket>(std::move(packetData));
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

    NetworkData packetData(std::move(packetBytes));
    return std::make_shared<AudioBundlePacket>(std::move(packetData));
}

}
