// 
// meta-fetcher.cpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "meta-fetcher.h"
#include <boost/thread.hpp>
#include <ndn-cpp/util/segment-fetcher.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "name-components.h"

using namespace ndn;
using namespace ndnrtc;

void
MetaFetcher::fetch(boost::shared_ptr<ndn::Face> f, boost::shared_ptr<ndn::KeyChain> kc,
	const ndn::Name& prefix, const OnMeta& onMeta, const OnError& onError)
{
	Interest i(Name(prefix).append(NameComponents::NameComponentMeta), 1000);

	isPending_ = true;
	boost::shared_ptr<MetaFetcher> me = boost::dynamic_pointer_cast<MetaFetcher>(shared_from_this());
	SegmentFetcher::fetch(*f, i, 
		kc.get(),
		[onMeta, me, this](const Blob& content, const std::vector<ValidationErrorInfo>& info){
			isPending_ = false;
            ImmutableHeaderPacket<DataSegmentHeader> packet(content);
			NetworkData nd(packet.getPayload().size(), packet.getPayload().data());
			onMeta(nd, info);
		},
		[onError, me, this](SegmentFetcher::ErrorCode code, const std::string& msg){
			isPending_ = false;
            onError(msg);
		});
}