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

namespace ndn {
	class Name;
	class Blob;
	class Face;
	class KeyChain;
    class Data;
}

namespace ndnrtc {
    // forward delcaration of typedef'ed template class
    struct Mutable;
    template <typename T>
    class NetworkDataT;
    typedef NetworkDataT<Mutable> NetworkDataAlias;

	/**
	 * MetaFetcher is a helper class to fetch metadata (stream metadata, manifests, etc.)
	 * which is segmented and named /<prefix>/<version>/<segment>.
	 * If data can not be verified using provided KeyChain object, it is still returned in 
	 * the callback alongside with an array of ValidationErrorInfo objects.
	 */
	class MetaFetcher : public NdnRtcComponent {
	public:
		typedef boost::function<void(NetworkDataAlias&, 
			const std::vector<ValidationErrorInfo>,
            const std::vector<boost::shared_ptr<ndn::Data>>& contentData)>
			OnMeta;
		typedef boost::function<void(const std::string&)>
			OnError;

		MetaFetcher(const boost::shared_ptr<ndn::Face>& face, const boost::shared_ptr<ndn::KeyChain>& keyChain):
            face_(face), keyChain_(keyChain){ description_ = "meta-fetcher"; }
        MetaFetcher(){ description_ = "meta-fetcher"; }

		void fetch(boost::shared_ptr<ndn::Face>, boost::shared_ptr<ndn::KeyChain>, 
			const ndn::Name&, const OnMeta&, const OnError&);
        void fetch(const ndn::Name&, const OnMeta&, const OnError&);

		bool hasPendingRequest() const { return isPending_; }

	private:
		bool isPending_;
        boost::shared_ptr<ndn::Face> face_;
        boost::shared_ptr<ndn::KeyChain> keyChain_;
	};
}

#endif