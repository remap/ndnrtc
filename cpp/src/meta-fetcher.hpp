// 
// meta-fetcher.hpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __meta_fetcher_h__
#define __meta_fetcher_h__

#include "segment-fetcher.hpp"
#include "ndnrtc-object.hpp"
#include "frame-data.hpp"

namespace ndn {
	class Name;
	class Blob;
	class Face;
	class KeyChain;
}

namespace ndnrtc {
	/**
	 * MetaFetcher is a helper class to fetch metadata (stream metadata, manifests, etc.)
	 * which is segmented and named /<prefix>/<version>/<segment>.
	 * If data can not be verified using provided KeyChain object, it is still returned in 
	 * the callback alongside with an array of ValidationErrorInfo objects.
	 */
	class MetaFetcher : public NdnRtcComponent {
	public:
		typedef boost::function<void(NetworkData&, 
			const std::vector<ValidationErrorInfo>)>
			OnMeta;
		typedef boost::function<void(const std::string&)>
			OnError;

		MetaFetcher(){ description_ = "meta-fetcher"; }
		void fetch(boost::shared_ptr<ndn::Face>, boost::shared_ptr<ndn::KeyChain>, 
			const ndn::Name&, const OnMeta&, const OnError&);
		bool hasPendingRequest() const { return isPending_; }

	private:
		bool isPending_;
	};
}

#endif