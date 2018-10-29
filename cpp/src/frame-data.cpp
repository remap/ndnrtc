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
boost::shared_ptr<VideoFramePacket>
VideoFramePacket::merge(const std::vector<ImmutableHeaderPacket<VideoFrameSegmentHeader>> &segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s : segments)
        packetBytes.insert(packetBytes.end(),
                           s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(boost::move(packetBytes));
    return boost::make_shared<VideoFramePacket>(boost::move(packetData));
}

//******************************************************************************
template <>
boost::shared_ptr<AudioBundlePacket>
AudioBundlePacket::merge(const std::vector<ImmutableHeaderPacket<DataSegmentHeader>> &segments)
{
    std::vector<uint8_t> packetBytes;
    for (auto s : segments)
        packetBytes.insert(packetBytes.end(),
                           s.getPayload().begin(), s.getPayload().end());

    NetworkData packetData(boost::move(packetBytes));
    return boost::make_shared<AudioBundlePacket>(boost::move(packetData));
}

// ****
// TBD: this needs to be re-designed :sigh: (the whole network/frame data thing)
boost::shared_ptr<WireSegment>
WireSegment::createSegment(const NamespaceInfo &namespaceInfo,
                           const boost::shared_ptr<ndn::Data> &data,
                           const boost::shared_ptr<const ndn::Interest> &interest)
{
    if (namespaceInfo.streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo &&
        (namespaceInfo.segmentClass_ == SegmentClass::Data || namespaceInfo.segmentClass_ == SegmentClass::Parity))
        return boost::make_shared<WireData<VideoFrameSegmentHeader>>(namespaceInfo, data, interest);

    return boost::make_shared<WireData<DataSegmentHeader>>(namespaceInfo, data, interest);
    ;
}

}
