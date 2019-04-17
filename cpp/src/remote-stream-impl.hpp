//
// remote-stream-impl.hpp
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_impl_h__
#define __remote_stream_impl_h__

#include <ndn-cpp/name.hpp>

#include "remote-stream.hpp"
#include "ndnrtc-object.hpp"
#include "meta-fetcher.hpp"
#include "data-validator.hpp"

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class SegmentController;
class BufferControl;
class PipelineControl;
class SampleEstimator;
class DrdEstimator;
class PipelineControlStateMachine;
class PipelineControl;
class ILatencyControl;
class IInterestControl;
class IBuffer;
class IPipeliner;
class IInterestQueue;
class IPlaybackQueue;
class IPlayout;
class IPlayoutControl;
class MediaStreamMeta;
class RetransmissionController;

/**
 * RemoteStreamImpl is a base class for implementing remote stream functionality
 */
class RemoteStreamImpl : public NdnRtcComponent
{
  public:
    RemoteStreamImpl(boost::asio::io_service &io,
                     const std::shared_ptr<ndn::Face> &face,
                     const std::shared_ptr<ndn::KeyChain> &keyChain,
                     const std::string &streamPrefix);

    bool isMetaFetched() const;
    std::vector<std::string> getThreads() const;

    void fetchMeta();
    void start(const std::string &threadName);
    void setThread(const std::string &threadName);
    std::string getThread() const { return threadName_; }
    void stop();

    void setInterestLifetime(unsigned int lifetimeMs);
    void setTargetBufferSize(unsigned int bufferSizeMs);
    void setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger);

    bool isVerified() const;
    bool isRunning() const { return isRunning_; };
    void attach(IRemoteStreamObserver *observer);
    void detach(IRemoteStreamObserver *observer);

    void setNeedsMeta(bool needMeta) { needMeta_ = needMeta; }
    statistics::StatisticsStorage getStatistics() const;
    ndn::Name getStreamPrefix() const;

  protected:
    MediaStreamParams::MediaStreamType type_;
    boost::asio::io_service &io_;
    bool needMeta_, isRunning_, cuedToRun_;
    int64_t metadataRequestedMs_;
    std::shared_ptr<ndn::Face> face_;
    std::shared_ptr<ndn::KeyChain> keyChain_;
    ndn::Name streamPrefix_;
    std::string threadName_;
    std::shared_ptr<statistics::StatisticsStorage> sstorage_;

    std::vector<IRemoteStreamObserver *> observers_;
    std::shared_ptr<MetaFetcher> metaFetcher_;
    std::shared_ptr<MediaStreamMeta> streamMeta_;
    std::map<std::string, std::shared_ptr<NetworkData>> threadsMeta_;

    std::shared_ptr<IBuffer> buffer_;
    std::shared_ptr<DrdEstimator> drdEstimator_;
    std::shared_ptr<SegmentController> segmentController_;
    std::shared_ptr<PipelineControl> pipelineControl_;
    std::shared_ptr<BufferControl> bufferControl_;
    std::shared_ptr<SampleEstimator> sampleEstimator_;
    std::shared_ptr<IPlayoutControl> playoutControl_;
    std::shared_ptr<IInterestQueue> interestQueue_;
    std::shared_ptr<IPipeliner> pipeliner_;
    std::shared_ptr<ILatencyControl> latencyControl_;
    std::shared_ptr<IInterestControl> interestControl_;
    std::shared_ptr<IPlayout> playout_;
    std::shared_ptr<IPlaybackQueue> playbackQueue_;
    std::shared_ptr<RetransmissionController> rtxController_;

    std::vector<ValidationErrorInfo> validationInfo_;

    void fetchThreadMeta(const std::string &threadName, const int64_t& metadataRequestedMs);
    void streamMetaFetched(NetworkData &);
    void threadMetaFetched(const std::string &thread, NetworkData &);
    virtual void initiateFetching();
    virtual void stopFetching();
    void addValidationInfo(const std::vector<ValidationErrorInfo> &);
    void notifyObservers(RemoteStream::Event ev);
};
}

#endif
