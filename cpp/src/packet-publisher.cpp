// 
// packet-publisher.cpp
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <ndn-cpp/c/common.h>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "packet-publisher.h"
#include "frame-data.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace ndn;

template<typename SegmentType, typename Settings>
PacketPublisher<SegmentType, Settings>::PacketPublisher(const Settings& settings, 
	const Name& nameFilter):
settings_(settings)
{
	settings_.memoryCache_->setInterestFilter(nameFilter, settings_.memoryCache_->getStorePendingInterest());
}

template<typename SegmentType, typename Settings>
size_t PacketPublisher<SegmentType, Settings>::publish(const Name& name, 
	const NetworkData& data, _DataSegmentHeader& commonHeader)
{
	std::vector<boost::shared_ptr<const MemoryContentCache::PendingInterest>> pendingInterests;
	settings_.memoryCache_->getPendingInterestsForName(name, pendingInterests);

	if (pendingInterests.size())
	{
		commonHeader.interestNonce_ = *(uint32_t *)(pendingInterests.back()->getInterest()->getNonce().buf());
		commonHeader.interestArrivalMs_ = pendingInterests.back()->getTimeoutPeriodStart();
		commonHeader.generationDelayMs_ = ndn_getNowMilliseconds()-pendingInterests.back()->getTimeoutPeriodStart();
	}

	std::vector<SegmentType> segments = SegmentType::slice(data, settings_.segmentWireLength_);

	unsigned int segIdx = 0;
	for (auto segment:segments)
	{
		segment.setHeader(commonHeader);

		Name segmentName(name);
		segmentName.appendSegment(segIdx);

		Data ndnSegment(segmentName);
		ndnSegment.getMetaInfo().setFreshnessPeriod(settings_.freshnessPeriodMs_);

		boost::shared_ptr<NetworkData> segmentData = segment.getNetworkData();

		ndnSegment.setContent(segmentData->getData(), segment.size());
		settings_.keyChain_->sign(ndnSegment);
		settings_.memoryCache_->add(ndnSegment);

		++segIdx;
	}

	return segments.size();
}


typedef PacketPublisher<VideoFrameSegment, PublisherSettings> VideoPacketPublisher;
typedef PacketPublisher<CommonSegment, PublisherSettings> CommonPacketPublisher;