//
// packets.cpp
//
//  Created by Peter Gusev on 14 March 2019.
//  Copyright 2019 Regents of the University of California
//

#include "packets.hpp"

#include "name-components.hpp"
#include "network-data.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;

namespace ndnrtc {
namespace packets {

boost::shared_ptr<BasePacket>
BasePacket::ndnrtcPacketFromRequest(const DataRequest& request)
{

    const NamespaceInfo& ninfo = request.getNamespaceInfo();

    switch (ninfo.segmentClass_)
    {
        case SegmentClass::Meta: return boost::make_shared<Meta>(ninfo, request.getData());
        case SegmentClass::Pointer: return boost::make_shared<Pointer>(ninfo, request.getData());
        case SegmentClass::Manifest: return boost::make_shared<Manifest>(ninfo, request.getData());
        case SegmentClass::Data:  // fall through
        case SegmentClass::Parity: return boost::make_shared<Segment>(ninfo, request.getData());
        default:
            break;
    }

    return boost::shared_ptr<BasePacket>();
}

BasePacket::BasePacket(const NamespaceInfo& ninfo, const boost::shared_ptr<const ndn::Data>& data) : data_(data) {}

//***
Meta::Meta(const NamespaceInfo& ninfo, const boost::shared_ptr<const ndn::Data>& data)
: BasePacket(ninfo, data)
{
    assert(ninfo.segmentClass_ == SegmentClass::Meta);

    if (ninfo.hasSeqNo_) // frame meta
    {
        contentMetaInfo_.wireDecode(data->getContent().buf(),
                                    data->getContent().size());
        assert(frameMeta_.ParseFromArray(contentMetaInfo_.getOther().buf(),
                                         contentMetaInfo_.getOther().size()));
    }
    else if (ninfo.isLiveMeta_)
    {
        assert(liveMeta_.ParseFromArray(data->getContent().buf(),
                                        data->getContent().size()));
    }
    else
    {
        assert(streamMeta_.ParseFromArray(data->getContent().buf(),
                                          data->getContent().size()));
    }
}

//***
Pointer::Pointer(const NamespaceInfo& ninfo, const boost::shared_ptr<const ndn::Data>& data)
: BasePacket(ninfo, data)
{
    set_.wireDecode(data_->getContent().buf(), data_->getContent().size());
}

//***
Manifest::Manifest(const NamespaceInfo& ninfo, const boost::shared_ptr<const ndn::Data>& data)
: BasePacket(ninfo, data)
{}

bool
Manifest::hasData(Data& d) const
{
    return SegmentsManifest::hasData(*data_, d);
}

size_t
Manifest::getSize() const
{
    return data_->getContent().size() / SegmentsManifest::DigestSize;
}

Segment::Segment(const NamespaceInfo& ninfo, const boost::shared_ptr<const ndn::Data>& data)
: BasePacket(ninfo, data)
{
    assert(ninfo.hasSegNo_);
    totalSegmentsNum_ = data_->getMetaInfo().getFinalBlockId().toSegment()+1;
}

}
}
