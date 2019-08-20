//
// packets.hpp
//
//  Created by Peter Gusev on 14 March 2019.
//  Copyright 2019 Regents of the University of California
//

#ifndef __packets_hpp__
#define __packets_hpp__


#include <ndn-cpp/delegation-set.hpp>
#include <ndn-cpp-tools/usersync/content-meta-info.hpp>

#include "proto/ndnrtc.pb.h"
#include "name-components.hpp"

namespace ndn
{
class Name;
class Data;
}

namespace ndnrtc
{

class NamespaceInfo;
class DataRequest;

namespace packets
{

class BasePacket {
public:
    static std::shared_ptr<BasePacket> ndnrtcPacketFromRequest(const DataRequest& request);
    virtual ~BasePacket(){}

protected:
    std::shared_ptr<const ndn::Data> data_;

    BasePacket(const NamespaceInfo& ninfo,
               const std::shared_ptr<const ndn::Data>& data);
};

class Meta : public BasePacket {
public:
    Meta(const NamespaceInfo& ninfo,
         const std::shared_ptr<const ndn::Data>& data);

    const StreamMeta& getStreamMeta() const { return streamMeta_; }
    const LiveMeta& getLiveMeta() const { return liveMeta_; }
    const ndntools::ContentMetaInfo& getContentMetaInfo() const
            { return contentMetaInfo_; }
    const FrameMeta& getFrameMeta() const { return frameMeta_; }

private:
    StreamMeta streamMeta_;
    LiveMeta liveMeta_;
    ndntools::ContentMetaInfo contentMetaInfo_;
    FrameMeta frameMeta_;
};

class Pointer : public BasePacket {
public:
    Pointer(const NamespaceInfo& ninfo,
            const std::shared_ptr<const ndn::Data>& data);

    const ndn::DelegationSet& getDelegationSet() const { return set_; }

private:
    ndn::DelegationSet set_;
};

class Manifest : public BasePacket {
public:
    Manifest(const NamespaceInfo& ninfo,
             const std::shared_ptr<const ndn::Data>& data);

    bool hasData(ndn::Data& d) const;
    size_t getSize() const;

private:
};

class Segment : public BasePacket {
public:
    Segment(const NamespaceInfo& ninfo,
            const std::shared_ptr<const ndn::Data>& data);

    size_t getTotalSegmentsNum() const { return totalSegmentsNum_; }

private:
    size_t totalSegmentsNum_;
};

} // packets
} // ndnrtc

#endif
