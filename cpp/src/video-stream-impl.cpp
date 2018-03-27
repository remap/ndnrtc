//
// video-stream-impl.cpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#define BOOST_THREAD_PROVIDES_FUTURE
#include <boost/thread/future.hpp>
#include <boost/asio.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <ndn-cpp/c/common.h>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <boost/thread/lock_guard.hpp>

#include "video-stream-impl.hpp"
#include "frame-data.hpp"
#include "video-thread.hpp"
#include "video-coder.hpp"
#include "packet-publisher.hpp"
#include "name-components.hpp"
#include "simple-log.hpp"
#include "estimators.hpp"
#include "clock.hpp"
#include "async.hpp"
#include "params.hpp"

#define PARITY_RATIO 0.2

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace std;
using namespace ndn;
using namespace estimators;

typedef boost::shared_ptr<VideoFramePacket> FramePacketPtr;
typedef boost::future<FramePacketPtr> FutureFrame;
typedef boost::shared_ptr<boost::future<FramePacketPtr>> FutureFramePtr;

VideoStreamImpl::VideoStreamImpl(const std::string &streamPrefix,
                                 const MediaStreamSettings &settings, bool useFec)
    : MediaStreamBase(streamPrefix, settings),
      playbackCounter_(0),
      fecEnabled_(useFec),
      busyPublishing_(0)
{
    if (settings_.params_.type_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
        throw runtime_error("Wrong media stream parameters type supplied (audio instead of video)");

    description_ = "vstream-" + settings_.params_.streamName_;

    for (int i = 0; i < settings_.params_.getThreadNum(); ++i)
        if (settings_.params_.getVideoThread(i))
            add(settings_.params_.getVideoThread(i));

    PublisherSettings ps;
    ps.sign_ = false; // stream samples are not signed - we use manifests for verification
    ps.keyChain_ = settings_.keyChain_;
    ps.memoryCache_ = cache_.get();
    ps.segmentWireLength_ = settings_.params_.producerParams_.segmentSize_;
    ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshness_.sampleMs_;
    ps.statStorage_ = statStorage_.get();

    framePublisher_ = boost::make_shared<VideoPacketPublisher>(ps);
    framePublisher_->setDescription("seg-publisher-" + settings_.params_.streamName_);
}

VideoStreamImpl::~VideoStreamImpl()
{
    // cleanup
}

vector<string> VideoStreamImpl::getThreads() const
{
    boost::lock_guard<boost::mutex> scopedLock(internalMutex_);
    std::vector<string> threads;

    for (auto it : threads_)
        threads.push_back(it.first);

    return threads;
}

int VideoStreamImpl::incomingFrame(const ArgbRawFrameWrapper &w)
{
    LogDebugC << "⤹ incoming ARGB frame " << w.width_ << "x" << w.height_ << std::endl;
    if (feedFrame(conv_ << w))
        return (playbackCounter_ - 1);
    return -1;
}

int VideoStreamImpl::incomingFrame(const I420RawFrameWrapper &w)
{
    LogDebugC << "⤹ incoming I420 frame " << w.width_ << "x" << w.height_ << std::endl;
    if (feedFrame(conv_ << w))
        return (playbackCounter_ - 1);
    return -1;
}

int VideoStreamImpl::incomingFrame(const YUV_NV21FrameWrapper &w)
{
    LogDebugC << "⤹ incoming NV21 frame " << w.width_ << "x" << w.height_ << std::endl;
    if (feedFrame(conv_ << w))
        return (playbackCounter_ - 1);
    return -1;
}

void VideoStreamImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    boost::lock_guard<boost::mutex> scopedLock(internalMutex_);
    MediaStreamBase::setLogger(logger);

    for (auto t : threads_)
        t.second->setLogger(logger);
    framePublisher_->setLogger(logger);
    metadataPublisher_->setLogger(logger);
    ILoggingObject::setLogger(logger);
}

//******************************************************************************
void VideoStreamImpl::add(const MediaThreadParams *mp)
{
    const VideoThreadParams *params = static_cast<const VideoThreadParams *>(mp);
    // check if thread already exists
    if (threads_.find(params->threadName_) != threads_.end())
    {
        stringstream ss;
        ss << "Thread " << params->threadName_ << " has been added already";
        throw runtime_error(ss.str());
    }
    else
    {
        boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

        threads_[params->threadName_] = boost::make_shared<VideoThread>(params->coderParams_);
        scalers_[params->threadName_] = boost::make_shared<FrameScaler>(params->coderParams_.encodeWidth_,
                                                                        params->coderParams_.encodeHeight_);
        seqCounters_[params->threadName_].first = 0;
        seqCounters_[params->threadName_].second = 0;
        metaKeepers_[params->threadName_] = boost::make_shared<MetaKeeper>(params);

        threads_[params->threadName_]->setDescription("thread-" + params->threadName_);
    }

    LogTraceC << "added thread " << params->threadName_ << std::endl;
}

void VideoStreamImpl::remove(const string &threadName)
{
    if (threads_.find(threadName) != threads_.end())
    {
        boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

        threads_.erase(threadName);
        scalers_.erase(threadName);
        seqCounters_.erase(threadName);
        metaKeepers_.erase(threadName);

        LogTraceC << "remove thread " << threadName << std::endl;
    }
}

bool VideoStreamImpl::feedFrame(const WebRtcVideoFrame &frame)
{
    (*statStorage_)[Indicator::CapturedNum]++;

    if (busyPublishing_ > 0)
    {
        LogWarnC << "⨂ busy publishing (capture rate may be too high)" << std::endl;
        return false;
    }

    if (threads_.size())
    {
        boost::lock_guard<boost::mutex> scopedLock(internalMutex_);
        LogDebugC << "↓ feeding " << playbackCounter_ << "p into encoders..." << std::endl;

        map<string, FutureFramePtr> futureFrames;
        for (auto it : threads_)
        {
            FutureFramePtr ff =
                boost::make_shared<FutureFrame>(boost::move(boost::async(boost::launch::async,
                                                                         boost::bind(&VideoThread::encode, it.second.get(), 
                                                                         (*scalers_[it.first])(frame)))));
            futureFrames[it.first] = ff;
        }

        map<string, FramePacketPtr> frames;
        for (auto it : futureFrames)
        {
            FramePacketPtr f(it.second->get());
            if (f.get())
            {
                (*statStorage_)[Indicator::EncodedNum]++;
                frames[it.first] = f;
            }
        }

        (*statStorage_)[Indicator::DroppedNum] += (threads_.size() - frames.size());
        bool result = false;

        if (frames.size())
        {
            publish(frames);
            playbackCounter_++;
            result = true;
        }

        if (!isPeriodicInvocationSet())
        {
            boost::shared_ptr<VideoStreamImpl> me = boost::dynamic_pointer_cast<VideoStreamImpl>(shared_from_this());
            setupInvocation(MediaStreamBase::MetaCheckIntervalMs,
                            boost::bind(&VideoStreamImpl::periodicInvocation, me));
        }

        return result;
    }
    else
        LogWarnC << "incoming frame was given, but there are no threads" << std::endl;

    return false;
}

void VideoStreamImpl::publish(map<string, FramePacketPtr> &frames)
{
    LogTraceC << "will publish " << frames.size() << " frames" << std::endl;

    for (auto it : frames)
    {
        // prepare packet header
        bool isKey = (it.second->getFrame()._frameType == webrtc::kVideoFrameKey);
        CommonHeader packetHdr;
        packetHdr.sampleRate_ = metaKeepers_[it.first]->getRate();
        packetHdr.publishTimestampMs_ = clock::millisecondTimestamp();
        packetHdr.publishUnixTimestamp_ = clock::unixTimestamp();

        it.second->setSyncList(getCurrentSyncList(isKey));
        it.second->setHeader(packetHdr);

        LogTraceC << "thread " << it.first << " " << packetHdr.sampleRate_
                  << "fps " << packetHdr.publishTimestampMs_ << "ms " << std::endl;

        publish(it.first, it.second);

        if (isKey)
            seqCounters_[it.first].first++;
        else
            seqCounters_[it.first].second++;
    }
}

void VideoStreamImpl::publish(const string &thread, FramePacketPtr &fp)
{
    boost::shared_ptr<NetworkData> parityData = fp->getParityData(
        VideoFrameSegment::payloadLength(settings_.params_.producerParams_.segmentSize_),
        PARITY_RATIO);

    bool isKey = (fp->getFrame()._frameType == webrtc::kVideoFrameKey);
    PacketNumber seqNo = (isKey ? seqCounters_[thread].first : seqCounters_[thread].second);
    PacketNumber pairedSeq = (isKey ? seqCounters_[thread].second : seqCounters_[thread].first);
    PacketNumber playbackNo = playbackCounter_;
    unsigned char gopPos = (char)threads_[thread]->getCoder().getGopCounter();
    Name dataName(streamPrefix_);
    dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);

    size_t nDataSeg = VideoFrameSegment::numSlices(*fp,
                                                   settings_.params_.producerParams_.segmentSize_);
    size_t nParitySeg = VideoFrameSegment::numSlices(*parityData,
                                                     settings_.params_.producerParams_.segmentSize_);
    boost::shared_ptr<VideoStreamImpl> me = boost::static_pointer_cast<VideoStreamImpl>(shared_from_this());
    boost::shared_ptr<MetaKeeper> keeper = metaKeepers_[thread];

    LogTraceC << "spawned publish task for "
              << seqNo
              << (isKey ? "k " : "d ")
              << playbackNo << "p "
              << "(" << SAMPLE_SUFFIX(dataName) << ")" << std::endl;

    busyPublishing_++;
    async::dispatchAsync(settings_.faceIo_, [me, nParitySeg, nDataSeg, seqNo, pairedSeq, keeper, isKey,
                                             thread, fp, parityData, dataName, playbackNo, gopPos, this] {
        VideoFrameSegmentHeader segmentHdr;
        segmentHdr.totalSegmentsNum_ = nDataSeg;
        segmentHdr.paritySegmentsNum_ = nParitySeg;
        segmentHdr.playbackNo_ = playbackNo;
        segmentHdr.pairedSequenceNo_ = pairedSeq;

        PublishedDataPtrVector segments =
            me->framePublisher_->publish(dataName, *fp, segmentHdr,
                                         (isKey ? settings_.params_.producerParams_.freshness_.sampleKeyMs_ : -1),
                                         isKey, true);
        assert(segments.size());
        keeper->updateMeta(isKey, nDataSeg, nParitySeg, seqNo, pairedSeq, gopPos);

        LogDebugC << "↓ published "
                  << seqNo << (isKey ? "k " : "d ") << playbackNo << "p "
                  << "(" << SAMPLE_SUFFIX(dataName) << ")x" << segments.size()
                  << " Dgen " << segmentHdr.generationDelayMs_ << "ms" << std::endl;

        PublishedDataPtrVector paritySegments;
        if (nParitySeg)
        {
            Name parityName(dataName);
            parityName.append(NameComponents::NameComponentParity);

            paritySegments =
                me->framePublisher_->publish(parityName, *parityData, segmentHdr,
                                             (isKey ? settings_.params_.producerParams_.freshness_.sampleKeyMs_ : -1),
                                             isKey);
            assert(paritySegments.size());
            std::copy(paritySegments.begin(), paritySegments.end(), std::back_inserter(segments));

            LogDebugC << "↓ published "
                      << seqNo << (isKey ? "k " : "d ") << playbackNo << "p "
                      << "(" << PARITY_SUFFIX(parityName) << ")x" << paritySegments.size()
                      << std::endl;
        }
        publishManifest(dataName, segments);
        busyPublishing_--;

        LogInfoC << "▻ published frame "
                 << seqNo << (isKey ? "k " : "d ") << playbackNo << "p "
                 << " data segments x" << segments.size()
                 << " parity segments x" << paritySegments.size()
                 << std::endl;

        (*statStorage_)[Indicator::PublishedNum]++;
        if (isKey)
            (*statStorage_)[Indicator::PublishedKeyNum]++;
        if (busyPublishing_ == 0)
            (*statStorage_)[Indicator::ProcessedNum]++;
    });
}

void VideoStreamImpl::publishManifest(ndn::Name dataName, PublishedDataPtrVector &segments)
{
    Manifest m(segments);
    dataName.append(NameComponents::NameComponentManifest).appendVersion(0);
    PublishedDataPtrVector ss = metadataPublisher_->publish(dataName, m);

    LogDebugC << (busyPublishing_ == 1 ? "⤷" : "↓")
              << " published manifest ☆ (" << dataName.getSubName(-5, 5) << ")x"
              << ss.size() << std::endl;
}

map<string, PacketNumber>
VideoStreamImpl::getCurrentSyncList(bool forKey)
{
    map<string, PacketNumber> syncList;
    for (auto it : seqCounters_)
        syncList[it.first] = (forKey ? it.second.first : it.second.second);
    return syncList;
}

bool VideoStreamImpl::updateMeta()
{
    boost::lock_guard<boost::mutex> scopedLock(internalMutex_);

    for (auto it : metaKeepers_)
    {
        Name metaName(streamPrefix_);
        // TODO: appendVersion() should probably be gone once SegemntFetcher
        // is updated to work without version number
        metaName.append(it.first).append(NameComponents::NameComponentMeta).appendVersion(0);;
        metadataPublisher_->publish(metaName, it.second->getMeta());
    }

    return false;
}

//******************************************************************************
VideoStreamImpl::MetaKeeper::MetaKeeper(const VideoThreadParams *params)
    : BaseMetaKeeper(params),
      rateMeter_(FreqMeter(boost::make_shared<TimeWindow>(1000))),
      deltaData_(Average(boost::make_shared<TimeWindow>(100))),
      deltaParity_(Average(boost::make_shared<TimeWindow>(100))),
      keyData_(Average(boost::make_shared<SampleWindow>(2))),
      keyParity_(Average(boost::make_shared<SampleWindow>(2)))
{
}

VideoStreamImpl::MetaKeeper::~MetaKeeper()
{
}

void VideoStreamImpl::MetaKeeper::updateMeta(bool isKey, size_t nDataSeg, size_t nParitySeg,
                                             PacketNumber seqNo, PacketNumber pairedSeqNo, unsigned char gopPos)
{
    Average &dataAvg = (isKey ? keyData_ : deltaData_);
    Average &parityAvg = (isKey ? keyParity_ : deltaParity_);

    double r = rateMeter_.value();
    double d = deltaData_.value();
    double dp = deltaParity_.value();
    double k = keyData_.value();
    double kp = keyParity_.value();

    rateMeter_.newValue(0);
    dataAvg.newValue(nDataSeg);
    parityAvg.newValue(nParitySeg);
    seqNo_.first = (isKey ? pairedSeqNo : seqNo); // first is delta
    seqNo_.second = (isKey ? seqNo : pairedSeqNo); // second is key
    gopPos_ = gopPos;
}

VideoThreadMeta
VideoStreamImpl::MetaKeeper::getMeta() const
{
    FrameSegmentsInfo segInfo;
    segInfo.deltaAvgSegNum_ = deltaData_.value();
    segInfo.deltaAvgParitySegNum_ = deltaParity_.value();
    segInfo.keyAvgSegNum_ = keyData_.value();
    segInfo.keyAvgParitySegNum_ = keyParity_.value();

    return boost::move(VideoThreadMeta(rateMeter_.value(), seqNo_.first, seqNo_.second, gopPos_,
                                       segInfo, ((VideoThreadParams *)params_)->coderParams_));
}

double
VideoStreamImpl::MetaKeeper::getRate() const
{
    return rateMeter_.value();
}
