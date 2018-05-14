//
// media-stream-base.cpp
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread/lock_guard.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "media-stream-base.hpp"
#include "name-components.hpp"
#include "async.hpp"
#include "clock.hpp"
#include "statistics.hpp"

#define META_CHECK_INTERVAL_MS 10

using namespace ndnrtc;
using namespace std;
using namespace ndn;

// how often is thread meta information published
const unsigned int MediaStreamBase::MetaCheckIntervalMs = META_CHECK_INTERVAL_MS;

MediaStreamBase::MediaStreamBase(const std::string &basePrefix,
                                 const MediaStreamSettings &settings)
    : Periodic(settings.faceIo_),
      basePrefix_(basePrefix),
      settings_(settings),
      streamPrefix_(NameComponents::streamPrefix(settings.params_.type_, basePrefix)),
      statStorage_(statistics::StatisticsStorage::createProducerStatistics())
{
    assert(settings_.face_);
    assert(settings_.keyChain_);
    
    streamTimestamp_ = (uint64_t)clock::millisecSinceEpoch();
    streamPrefix_.append(Name(settings_.params_.streamName_));
    streamPrefix_.appendTimestamp(streamTimestamp_);

    unsigned int minimumFreshness = min(settings_.params_.producerParams_.freshness_.metadataMs_, 
                                        settings_.params_.producerParams_.freshness_.sampleMs_);
    minimumFreshness = min(minimumFreshness, settings_.params_.producerParams_.freshness_.sampleKeyMs_);
    // set cleanup interval for memory cache to 0
    // this will make cache to cleanup every time there's an incoming interest 
    // or data added (and prevent it from hanging for few ms when data rate is high)
    cache_ = boost::make_shared<MemoryContentCache>(settings_.face_, 0);
    cache_->setMinimumCacheLifetime(1000);
    // set filter for prefix without the timestamp, because stream _meta is served there
    cache_->setInterestFilter(streamPrefix_.getPrefix(-1), cache_->getStorePendingInterest());

    PublisherSettings ps;
    ps.sign_ = true; // it's ok to sign every packet as data publisher
                     // is used for low-rate data (max 10fps) and manifests
    ps.keyChain_ = settings_.keyChain_;
    ps.memoryCache_ = cache_.get();
    ps.segmentWireLength_ = MAX_NDN_PACKET_SIZE; // it's ok to rely on link-layer fragmenting
                                                 // because data is low-rate
    ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshness_.metadataMs_;
    ps.statStorage_ = statStorage_.get();

    metadataPublisher_ = boost::make_shared<CommonPacketPublisher>(ps);
    metadataPublisher_->setDescription("metadata-publisher-" + settings_.params_.streamName_);
}

MediaStreamBase::~MediaStreamBase()
{
    // cleanup
}

void MediaStreamBase::addThread(const MediaThreadParams *params)
{
    add(params);
    publishMeta();
}

void MediaStreamBase::removeThread(const string &threadName)
{
    remove(threadName);
    publishMeta();
}

statistics::StatisticsStorage
MediaStreamBase::getStatistics() const
{
    (*statStorage_)[statistics::Indicator::Timestamp] = clock::millisecondTimestamp();
    return *statStorage_;
}

void 
MediaStreamBase::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    metadataPublisher_->setLogger(logger);
}

void MediaStreamBase::publishMeta()
{
    boost::shared_ptr<MediaStreamMeta> meta(boost::make_shared<MediaStreamMeta>(streamTimestamp_, getThreads()));
    // don't need to synchronize unless publishMeta will be called
    // from places others than addThread/removeThread or outer class'
    // constructor LocalVideoStream/LocalAudioStream

    // use name without timestamp here
    Name metaName(streamPrefix_.getPrefix(-1));

    // TODO: appendVersion() should probably be gone once SegemntFetcher
    // is updated to work without version number
    metaName.append(NameComponents::NameComponentMeta).appendVersion(0);

    boost::shared_ptr<MediaStreamBase> me = 
        boost::static_pointer_cast<MediaStreamBase>(shared_from_this());

    async::dispatchAsync(settings_.faceIo_, [me, metaName, meta]() {
        me->metadataPublisher_->publish(metaName, *meta);
    });
}

unsigned int MediaStreamBase::periodicInvocation()
{
    publishMeta(); // republish stream metadata

    if (updateMeta()) // update thread meta
        return MetaCheckIntervalMs;
    return 0;
}
