//
// media-stream-base.hpp
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __media_stream_base_h__
#define __media_stream_base_h__

#include <boost/thread/mutex.hpp>
#include <ndn-cpp/name.hpp>

#include "local-stream.hpp"
#include "packet-publisher.hpp"
#include "periodic.hpp"
#include "statistics.hpp"

namespace ndn
{
class MemoryContentCache;
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class StorageEngine;
class MediaThreadParams;

class MediaStreamBase : public NdnRtcComponent,
                        public Periodic
{
  public:
    static const unsigned int MetaCheckIntervalMs;

    MediaStreamBase(const std::string &basePrefix,
                    const MediaStreamSettings &settings);
    virtual ~MediaStreamBase();

    void addThread(const MediaThreadParams *params);
    void removeThread(const std::string &threadName);

    std::string getPrefix() { return streamPrefix_.toUri(); }
    std::string getBasePrefix() const { return basePrefix_; }
    std::string getStreamName() const { return settings_.params_.streamName_; }

    boost::shared_ptr<StorageEngine> getStorage() const { return storage_; }

    virtual std::vector<std::string> getThreads() const = 0;
    statistics::StatisticsStorage getStatistics() const;
    virtual void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

    // callback for storing cached data into the storage
    void onSegmentsCached(PublishedDataPtrVector segments);

  protected:
    friend LocalAudioStream;
    friend LocalVideoStream;

    template <typename Meta>
    class BaseMetaKeeper
    {
      public:
        BaseMetaKeeper(const MediaThreadParams *params) : params_(params) {}
        virtual ~BaseMetaKeeper() {}

        virtual Meta getMeta() const = 0;
        virtual double getRate() const = 0;

      protected:
        const MediaThreadParams *params_;
    };

    mutable boost::mutex internalMutex_;
    MediaStreamSettings settings_;
    std::string basePrefix_;
    ndn::Name streamPrefix_;
    boost::shared_ptr<ndn::MemoryContentCache> cache_;
    boost::shared_ptr<CommonPacketPublisher> metadataPublisher_;
    boost::shared_ptr<statistics::StatisticsStorage> statStorage_;
    boost::shared_ptr<StorageEngine> storage_;
    uint64_t streamTimestamp_;
    uint32_t metaVersion_;

    virtual void add(const MediaThreadParams *params) = 0;
    virtual void remove(const std::string &threadName) = 0;
    void publishMeta();
    unsigned int periodicInvocation();
    virtual bool updateMeta() = 0;
};
}

#endif
