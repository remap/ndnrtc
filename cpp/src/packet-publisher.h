// 
// packet-publisher.h
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __packet_publisher_h__
#define __packet_publisher_h__

#include <boost/shared_ptr.hpp>
#include <ndn-cpp/c/common.h>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "frame-data.h"
#include "ndnrtc-object.h"

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

	template<typename KeyChain, typename MemoryCache>
	struct _PublisherSettings {
		KeyChain* keyChain_;
		MemoryCache* memoryCache_;
		size_t segmentWireLength_;
		unsigned int freshnessPeriodMs_;
	};

	typedef _PublisherSettings<ndn::KeyChain, ndn::MemoryContentCache> PublisherSettings;

	template<typename SegmentType, typename Settings>
	class PacketPublisher : public NdnRtcComponent {
	public:
		PacketPublisher(const Settings& settings, const ndn::Name& nameFilter = ndn::Name("/nofilter")):
		settings_(settings)
		{
			if (nameFilter.toUri() != "/nofilter")
				settings_.memoryCache_->setInterestFilter(nameFilter, settings_.memoryCache_->getStorePendingInterest());
		}

		size_t publish(const ndn::Name& name, const NetworkData& data)
		{
			// provide dummy memory of the size of the segment header to publish function
			// we don't care of bytes that will be saved in this memory, so allocate it
			// as shared_ptr so it's released automatically upon completion
			boost::shared_ptr<uint8_t[]> dummyHeader(new uint8_t[SegmentType::headerSize()]);
			memset(dummyHeader.get(), SegmentType::headerSize(), 0);
			return publish(name, data, (_DataSegmentHeader&)*dummyHeader.get());
		}

		size_t publish(const ndn::Name& name, const NetworkData& data, 
			_DataSegmentHeader& commonHeader)
		{
			LogDebugC << "publish " << name << std::endl;

			std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> pendingInterests;
			settings_.memoryCache_->getPendingInterestsForName(name, pendingInterests);
		
			if (pendingInterests.size())
			{
				commonHeader.interestNonce_ = *(uint32_t *)(pendingInterests.back()->getInterest()->getNonce().buf());
				commonHeader.interestArrivalMs_ = pendingInterests.back()->getTimeoutPeriodStart();
				commonHeader.generationDelayMs_ = ndn_getNowMilliseconds()-pendingInterests.back()->getTimeoutPeriodStart();
				
				LogDebugC << "pending interest found " << pendingInterests.back()->getInterest()->toUri() << std::endl;
				LogStatC << "Dgen" << STAT_DIV << commonHeader.generationDelayMs_ << std::endl;
			}
		
			std::vector<SegmentType> segments = SegmentType::slice(data, settings_.segmentWireLength_);
			
			LogTraceC << "sliced into " << segments.size() << " segments" << std::endl;

			unsigned int segIdx = 0;
			for (auto segment:segments)
			{
				segment.setHeader(commonHeader);
				boost::shared_ptr<NetworkData> segmentData = segment.getNetworkData();
		
				ndn::Name segmentName(name);
				segmentName.appendSegment(segIdx);
				segmentName.append(ndn::Name::Component::fromNumber(segmentData->getCrcValue()));
				
				ndn::Data ndnSegment(segmentName);
				ndnSegment.getMetaInfo().setFreshnessPeriod(settings_.freshnessPeriodMs_);
				ndnSegment.setContent(segmentData->getData(), segment.size());
				settings_.keyChain_->sign(ndnSegment);
				settings_.memoryCache_->add(ndnSegment);
		
				LogTraceC << "added to cache " << segmentName << std::endl;
				++segIdx;
			}

			LogDebugC << "published " << name << " (" << segments.size() << " segments)" << std::endl;

			return segments.size();
		}
	private:
		Settings settings_;
	};

	typedef PacketPublisher<VideoFrameSegment, PublisherSettings> VideoPacketPublisher;
	typedef PacketPublisher<CommonSegment, PublisherSettings> CommonPacketPublisher;
}

#endif