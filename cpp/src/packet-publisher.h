// 
// packet-publisher.h
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __packet_publisher_h__
#define __packet_publisher_h__

#include <boost/shared_ptr.hpp>

namespace ndn{
	class Face;
	class KeyChain;
	class MemoryContentCache;
	class Name;
	class Interest;
	class InterestFilter;
}

namespace ndnrtc{
	class NetworkData;
	struct _DataSegmentHeader;
	class VideoPacketPublisher;
	class CommonPacketPublisher;

	template<typename KeyChain, typename MemoryCache>
	struct _PublisherSettings {
		KeyChain* keyChain_;
		MemoryCache* memoryCache_;
		size_t segmentWireLength_;
		unsigned int freshnessPeriodMs_;
	};

	typedef _PublisherSettings<ndn::KeyChain, ndn::MemoryContentCache> PublisherSettings;

	template<typename SegmentType, typename Settings>
	class PacketPublisher {
	public:
		PacketPublisher(const Settings& settings, const ndn::Name& nameFilter);

		size_t publish(const ndn::Name& name, const NetworkData& data, 
			_DataSegmentHeader& commonHeader);
	private:
		Settings settings_;
	};
}

#endif