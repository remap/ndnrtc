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

	SegmentFetcher::fetch(*f, i, 
		kc.get(),
		// [kc](const ptr_lib::shared_ptr<Data>& data)->bool{
		// 	kc->verifyData(data, 
		// 		[](const boost::shared_ptr<Data>&)
		// 		{ std::cout << "verified" << std::endl; },
		// 		[](const boost::shared_ptr<Data>&)
		// 		{ std::cout << "verify failed" << std::endl; });

		// 	return true;
		// },
		[onMeta](const Blob& content){
			ImmutableHeaderPacket<DataSegmentHeader> packet(content);
			NetworkData nd(packet.getPayload().size(), packet.getPayload().data());
			onMeta(nd);
		},
		[onError](SegmentFetcher::ErrorCode code, const std::string& msg){
			onError(msg);
		});
}
