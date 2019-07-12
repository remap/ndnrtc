//
// packet-publisher.hpp
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

#include <ndn-cpp/key-locator.hpp>

#include "frame-data.hpp"
#include "ndnrtc-object.hpp"
#include "statistics.hpp"

#define ADD_CRC 0
// this number defines iteration when publisher will
// send NACKs to all pending interests, unsatisfied with data
#define FULL_PIT_FREQUENCY 1000

namespace ndn
{
class Face;
class KeyChain;
class MemoryContentCache;
class Name;
class Interest;
class InterestFilter;
}

namespace ndnrtc
{

static const size_t MAX_NDN_PACKET_SIZE = 8800;

namespace statistics
{
class StatisticsStorage;
}

template <typename T>
class NetworkDataT;
struct _DataSegmentHeader;
typedef NetworkDataT<Mutable> MutableNetworkData;
typedef std::vector<boost::shared_ptr<const ndn::Data>> PublishedDataPtrVector;
typedef boost::function<void(PublishedDataPtrVector)> OnSegmentsCached;

template <typename KeyChain, typename MemoryCache>
struct _PublisherSettings
{
    _PublisherSettings() : keyChain_(nullptr), memoryCache_(nullptr),
                           statStorage_(nullptr) {}

    KeyChain *keyChain_;
    MemoryCache *memoryCache_;
    statistics::StatisticsStorage *statStorage_;
    OnSegmentsCached onSegmentsCached_;
    size_t segmentWireLength_;
    unsigned int freshnessPeriodMs_;
    bool sign_ = true;
};

typedef _PublisherSettings<ndn::KeyChain, ndn::MemoryContentCache> PublisherSettings;

template <typename SegmentType, typename Settings>
class PacketPublisher : public NdnRtcComponent
{
  public:
    PacketPublisher(const Settings &settings) : settings_(settings), fullPitClean_(0)
    {
        assert(settings_.keyChain_);
        assert(settings_.memoryCache_);
        assert(settings_.statStorage_);
    }

    PublishedDataPtrVector publish(const ndn::Name &name, const MutableNetworkData &data,
                                   int freshnessMs = -1,
                                   bool forcePitClean = false, bool banPitClean = false)
    {
        // provide dummy memory of the size of the segment header to publish function
        // we don't care of bytes that will be saved in this memory, so allocate it
        // as shared_ptr so it's released automatically upon completion
        boost::shared_ptr<uint8_t[]> dummyHeader(new uint8_t[SegmentType::headerSize()]);
        memset(dummyHeader.get(), SegmentType::headerSize(), 0);
        return publish(name, data, (_DataSegmentHeader &)*dummyHeader.get(),
                       freshnessMs, forcePitClean, banPitClean);
    }

    PublishedDataPtrVector publish(const ndn::Name &name, const MutableNetworkData &data,
                                   _DataSegmentHeader &commonHeader, int freshnessMs,
                                   bool forcePitClean = false, bool banPitClean = false)
    {
        PublishedDataPtrVector ndnSegments;
        std::vector<SegmentType> segments = SegmentType::slice(data, settings_.segmentWireLength_);
        LogTraceC << "sliced into " << segments.size() << " segments" << std::endl;

        commonHeader.interestNonce_ = 0;
        commonHeader.generationDelayMs_ = 0;
        commonHeader.interestArrivalMs_ = 0;

        unsigned int segIdx = 0;
        freshnessMs = (freshnessMs == -1 ? settings_.freshnessPeriodMs_ : freshnessMs);

        for (auto segment : segments)
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
            ndnSegment->getMetaInfo().setFreshnessPeriod(freshnessMs);
            ndnSegment->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size() - 1));
            ndnSegment->setContent(segmentData->getData(), segment.size());
            sign(ndnSegment);
            settings_.memoryCache_->add(*ndnSegment);
            ++segIdx;
            ndnSegments.push_back(ndnSegment);

            (*settings_.statStorage_)[statistics::Indicator::BytesPublished] += ndnSegment->getContent().size();
            (*settings_.statStorage_)[statistics::Indicator::RawBytesPublished] += ndnSegment->getDefaultWireEncoding().size();

            LogTraceC << "cached " << segmentName << " ("
                      << ndnSegment->getContent().size() << "b payload, "
                      << ndnSegment->getDefaultWireEncoding().size() << "b wire, "
                      << ndnSegment->getMetaInfo().getFreshnessPeriod() << "ms fp)"
                      << std::endl;
        }

        if (!banPitClean)
            cleanPit(name, forcePitClean);

        (*settings_.statStorage_)[statistics::Indicator::PublishedSegmentsNum] += segments.size();

        if (settings_.onSegmentsCached_)
            settings_.onSegmentsCached_(ndnSegments);

        return ndnSegments;
    }

  private:
    Settings settings_;
    unsigned int fullPitClean_;

    void checkForPendingInterests(const ndn::Name &name, _DataSegmentHeader &commonHeader)
    {
        std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> pendingInterests;
        settings_.memoryCache_->getPendingInterestsForName(name, pendingInterests);

        if (pendingInterests.size())
        {
            commonHeader.interestNonce_ = *(uint32_t *)(pendingInterests.back()->getInterest()->getNonce().buf());
            commonHeader.interestArrivalMs_ = pendingInterests.back()->getTimeoutPeriodStart();
            commonHeader.generationDelayMs_ = ndn_getNowMilliseconds() - pendingInterests.back()->getTimeoutPeriodStart();

            (*settings_.statStorage_)[statistics::Indicator::InterestsReceivedNum] += pendingInterests.size();

            LogTraceC << "PIT hit " << pendingInterests.back()->getInterest()->toUri() << std::endl;
        }
    }

    void sign(boost::shared_ptr<ndn::Data> segment)
    {
        if (settings_.sign_)
        {
            settings_.keyChain_->sign(*segment);
            (*settings_.statStorage_)[statistics::Indicator::SignNum]++;
        }
        else
        {
            static uint8_t digest[ndn_SHA256_DIGEST_SIZE];
            memset(digest, 0, ndn_SHA256_DIGEST_SIZE);
            ndn::Blob signatureBits(digest, sizeof(digest));
            segment->setSignature(ndn::DigestSha256Signature());
            ndn::DigestSha256Signature *sha256Signature = (ndn::DigestSha256Signature *)segment->getSignature();
            sha256Signature->setSignature(signatureBits);
        }
    }

    /**
     * Retrieves all pending interests for given name and publishes application NACKs for them
     */
    void cleanPit(const ndn::Name &name, bool forceFullPitClean = false)
    {
        std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> pendingInterests;
        settings_.memoryCache_->getPendingInterestsWithPrefix(name, pendingInterests);

        if (pendingInterests.size())
        {
            LogTraceC
                << "cleaning PIT for " << name
                << " (sending NACKs for "
                << pendingInterests.size() << " interests)" << std::endl;

            for (auto pi : pendingInterests)
                publishNack(pi->getInterest()->getName());
        }
        else
            LogTraceC << "no pending for " << name << std::endl;

        if (fullPitClean_++ % FULL_PIT_FREQUENCY == 0 || forceFullPitClean)
        {
            if (!forceFullPitClean)
                fullPitClean_ = 0;
            deepCleanPit(name);
        }
    }

    void deepCleanPit(const ndn::Name &name)
    {
        NamespaceInfo info;

        if (!NameComponents::extractInfo(name, info))
        {
            LogErrorC << "can't extract info from " << name << std::endl;
            return;
        }

        if (info.isMeta_)
            return;

        std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> pendingInterests;

        // extract all pending interests for this stream
        settings_.memoryCache_->getPendingInterestsWithPrefix(info.getPrefix(prefix_filter::Stream), pendingInterests);

        if (pendingInterests.size())
        {
            for (auto pi : pendingInterests)
            {
                NamespaceInfo piInfo;

                if (NameComponents::extractInfo(pi->getInterest()->getName(), piInfo))
                {
                    // we are interested in older interests (those that request data that has already been published)
                    // this is needed to respond with NACKs, when consumer runs slightly behind producer
                    if (piInfo.class_ == info.class_ && piInfo.sampleNo_ < info.sampleNo_)
                    {
                        publishNack(pi->getInterest()->getName());
                        LogTraceC << "PIT deep clean " << pi->getInterest()->getName() << std::endl;
                    }
                }
                else
                    LogWarnC << "couldn't extract info from pending interest "
                             << pi->getInterest()->getName() << " this will time out" << std::endl;
            }
        }
    }

    void publishNack(const ndn::Name &name)
    {
        boost::shared_ptr<ndn::Data> nack(boost::make_shared<ndn::Data>(name));
        nack->getMetaInfo().setFreshnessPeriod(settings_.freshnessPeriodMs_);
        nack->setContent((const uint8_t *)"nack", 4);
        nack->getMetaInfo().setType(ndn_ContentType_NACK);
        // settings_.memoryCache_->add(*nack);
    }
};

typedef PacketPublisher<VideoFrameSegment, PublisherSettings> VideoPacketPublisher;
typedef PacketPublisher<CommonSegment, PublisherSettings> CommonPacketPublisher;
}

#endif
