// 
// meta-fetcher.h
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __meta_fetcher_h__
#define __meta_fetcher_h__

#include <ndn-cpp/util/segment-fetcher.hpp>

#include "ndnrtc-object.h"
#include "frame-data.h"

namespace ndn {
	class Name;
	class Blob;
	class Face;
	class KeyChain;
}

namespace ndnrtc {
	class MetaFetcher : public NdnRtcComponent{
	public:
		typedef boost::function<void(NetworkData&)>
			OnMeta;
		typedef boost::function<void(const std::string&)>
			OnError;

		void fetch(boost::shared_ptr<ndn::Face>, boost::shared_ptr<ndn::KeyChain>, 
			const ndn::Name&, const OnMeta&, const OnError&);

	private:
	};
}

#endif