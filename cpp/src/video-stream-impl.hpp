//
// video-stream-impl.hpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __video_stream_impl_h__
#define __video_stream_impl_h__

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/atomic.hpp>

#include "interfaces.hpp"
#include "media-stream-base.hpp"
#include "ndnrtc-object.hpp"
#include "packet-publisher.hpp"
#include "frame-converter.hpp"
#include "estimators.hpp"

namespace ndn
{
class MemoryContentCache;
}

namespace ndnlog
{
namespace new_api
{
class Logger;
}
}

namespace ndnrtc
{
class VideoThread;
class FrameScaler;
class VideoThreadParams;
struct Mutable;
template <typename T>
class VideoFramePacketT;
typedef VideoFramePacketT<Mutable> VideoFramePacketAlias;

class VideoStreamImpl : public MediaStreamBase
{
  public:
    VideoStreamImpl(const std::string &streamPrefix,
                    const MediaStreamSettings &settings, bool useFec);
    ~VideoStreamImpl();

    std::vector<std::string> getThreads() const override;

    int incomingFrame(const ArgbRawFrameWrapper &);
    int incomingFrame(const I420RawFrameWrapper &);
    int incomingFrame(const YUV_NV21FrameWrapper &);
    
    const std::map<std::string, FrameInfo>& getLastPublished() { return lastPublished_; }
    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger>) override;

  private:
    friend LocalVideoStream::LocalVideoStream(const std::string &, const MediaStreamSettings &, bool);
    friend LocalVideoStream::~LocalVideoStream();

    VideoStreamImpl(const VideoStreamImpl &stream) = delete;

    class MetaKeeper : public MediaStreamBase::BaseMetaKeeper<VideoThreadMeta>
    {
      public:
        MetaKeeper(const VideoThreadParams *params);
        ~MetaKeeper();

        VideoThreadMeta getMeta() const;
        double getRate() const;

        void updateMeta(bool isKey, size_t nDataSeg, size_t nParitySeg, 
                        PacketNumber seqNo, PacketNumber pairedSeqNo, unsigned char gopPos);

        uint32_t getVersionNumber() const { return versionNumber_; }

      private:
        MetaKeeper(const MetaKeeper &) = delete;

        estimators::FreqMeter rateMeter_;
        estimators::Average deltaData_, deltaParity_;
        estimators::Average keyData_, keyParity_;
        std::pair<PacketNumber, PacketNumber> seqNo_;
        unsigned char gopPos_;
        uint32_t versionNumber_;
    };

    bool fecEnabled_;
    boost::atomic<int> busyPublishing_;
    RawFrameConverter conv_;
    std::map<std::string, boost::shared_ptr<VideoThread>> threads_;
    std::map<std::string, boost::shared_ptr<FrameScaler>> scalers_;
    std::map<std::string, boost::shared_ptr<MetaKeeper>> metaKeepers_;
    std::map<std::string, std::pair<uint64_t, uint64_t>> seqCounters_;
    uint64_t playbackCounter_;
    boost::shared_ptr<VideoPacketPublisher> framePublisher_;
    std::map<std::string, FrameInfo> lastPublished_;

    void add(const MediaThreadParams *params) override;
    void remove(const std::string &threadName) override;
    bool updateMeta() override;

    bool feedFrame(const WebRtcVideoFrame &frame);
    void publish(std::map<std::string, boost::shared_ptr<VideoFramePacketAlias>> &frames);
    std::string publish(const std::string &thread, boost::shared_ptr<VideoFramePacketAlias> &fp);
    void publishManifest(ndn::Name dataName, PublishedDataPtrVector &segments);
    std::map<std::string, PacketNumber> getCurrentSyncList(bool forKey = false);
};
}

#endif