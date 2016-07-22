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
#include <ndn-cpp/digest-sha256-signature.hpp>

#include "frame-data.h"
#include "ndnrtc-object.h"
#include "statistics.h"

#define ADD_CRC 0

namespace ndn{
	class Face;
	class KeyChain;
	class MemoryContentCache;
	class Name;
	class Interest;
	class InterestFilter;
}

namespace ndnrtc {
	
	static const size_t MAX_NDN_PACKET_SIZE = 8800;

	namespace statistics {
		class StatisticsStorage;
	}

	template<typename T>
	class NetworkDataT;
	struct _DataSegmentHeader;
	typedef NetworkDataT<Mutable> MutableNetworkData;

	template<typename KeyChain, typename MemoryCache>
	struct _PublisherSettings {
        _PublisherSettings():keyChain_(nullptr), memoryCache_(nullptr),
            statStorage_(nullptr){}
        
		KeyChain* keyChain_;
		MemoryCache* memoryCache_;
		statistics::StatisticsStorage *statStorage_;
		size_t segmentWireLength_;
		unsigned int freshnessPeriodMs_;
		bool sign_ = true;
	};

	typedef _PublisherSettings<ndn::KeyChain, ndn::MemoryContentCache> PublisherSettings;
	typedef std::vector<boost::shared_ptr<const ndn::Data>> PublishedDataPtrVector;

	template<typename SegmentType, typename Settings>
	class PacketPublisher : public NdnRtcComponent {
	public:
		PacketPublisher(const Settings& settings):
        settings_(settings)
        {
            assert(settings_.keyChain_);
            assert(settings_.memoryCache_);
            assert(settings_.statStorage_);
        }

		PublishedDataPtrVector publish(const ndn::Name& name, const MutableNetworkData& data)
		{
			// provide dummy memory of the size of the segment header to publish function
			// we don't care of bytes that will be saved in this memory, so allocate it
			// as shared_ptr so it's released automatically upon completion
			boost::shared_ptr<uint8_t[]> dummyHeader(new uint8_t[SegmentType::headerSize()]);
			memset(dummyHeader.get(), SegmentType::headerSize(), 0);
			return publish(name, data, (_DataSegmentHeader&)*dummyHeader.get());
		}

		PublishedDataPtrVector publish(const ndn::Name& name, const MutableNetworkData& data, 
			_DataSegmentHeader& commonHeader)
		{
			PublishedDataPtrVector ndnSegments;
			std::vector<SegmentType> segments = SegmentType::slice(data, settings_.segmentWireLength_);
			LogTraceC << "sliced into " << segments.size() << " segments" << std::endl;

			unsigned int segIdx = 0;
			for (auto segment:segments)
			{
                ndn::Name segmentName(name);
                segmentName.appendSegment(segIdx);
                #if ADD_CRC
                segmentName.append(ndn::Name::Component::fromNumber(segmentData->getCrcValue()));
                #endif
                
                checkForPendingInterests(segmentName, commonHeader);
				segment.setHeader(commonHeader);
                
				boost::shared_ptr<MutableNetworkData> segmentData = segment.getNetworkData();
				boost::shared_ptr<ndn::Data> ndnSegment(boost::make_shared<ndn::Data>(segmentName));
				ndnSegment->getMetaInfo().setFreshnessPeriod(settings_.freshnessPeriodMs_);
				ndnSegment->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size()-1));
				ndnSegment->setContent(segmentData->getData(), segment.size());
				sign(ndnSegment);
				settings_.memoryCache_->add(*ndnSegment);
                ++segIdx;
                ndnSegments.push_back(ndnSegment);

                (*settings_.statStorage_)[statistics::Indicator::BytesPublished] += ndnSegment->getContent().size();
                (*settings_.statStorage_)[statistics::Indicator::RawBytesPublished] += ndnSegment->getDefaultWireEncoding().size();
		
				LogTraceC << "cached " << segmentName << " ("
						<< ndnSegment->getContent().size() << "b payload, "
						<< ndnSegment->getDefaultWireEncoding().size() << "b wire)" << std::endl;
			}
            
            (*settings_.statStorage_)[statistics::Indicator::PublishedSegmentsNum] += segments.size();
            #warning should return vector of pointers to Data objects
			return ndnSegments;
		}
	private:
		Settings settings_;
        
        void checkForPendingInterests(const ndn::Name& name, _DataSegmentHeader& commonHeader)
        {
            commonHeader.interestNonce_ = 0;
            commonHeader.generationDelayMs_ = 0;
            commonHeader.interestArrivalMs_ = 0;
            
            std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> pendingInterests;
            settings_.memoryCache_->getPendingInterestsForName(name, pendingInterests);
            
            if (pendingInterests.size())
            {
                commonHeader.interestNonce_ = *(uint32_t *)(pendingInterests.back()->getInterest()->getNonce().buf());
                commonHeader.interestArrivalMs_ = pendingInterests.back()->getTimeoutPeriodStart();
                commonHeader.generationDelayMs_ = ndn_getNowMilliseconds()-pendingInterests.back()->getTimeoutPeriodStart();
                
                (*settings_.statStorage_)[statistics::Indicator::InterestsReceivedNum] += pendingInterests.size();
                
                LogTraceC << "PIT hit " << pendingInterests.back()->getInterest()->toUri() << std::endl;
            }
        }

        void sign(boost::shared_ptr<ndn::Data> segment)
        {
			if (settings_.sign_) 
				settings_.keyChain_->sign(*segment);
			else
			{
				static uint8_t digest[ndn_SHA256_DIGEST_SIZE];
				memset(digest, 0, ndn_SHA256_DIGEST_SIZE);
				ndn::Blob signatureBits(digest, sizeof(digest));
				segment->setSignature(ndn::DigestSha256Signature());
				ndn::DigestSha256Signature* sha256Signature = (ndn::DigestSha256Signature*)segment->getSignature();
				sha256Signature->setSignature(signatureBits);
			}
        }
	};

	typedef PacketPublisher<VideoFrameSegment, PublisherSettings> VideoPacketPublisher;
	typedef PacketPublisher<CommonSegment, PublisherSettings> CommonPacketPublisher;
}

#endif