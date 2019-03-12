//
// remote-video-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-video-stream.hpp"
#include <ndn-cpp/name.hpp>
// #include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "interfaces.hpp"
#include "frame-data.hpp"
#include "video-playout.hpp"
#include "pipeline-control.hpp"
#include "pipeliner.hpp"
#include "latency-control.hpp"
#include "interest-control.hpp"
#include "playout-control.hpp"
#include "sample-estimator.hpp"
#include "sample-validator.hpp"
// #include "video-decoder.hpp"
#include "clock.hpp"

using namespace ndnrtc;
using namespace ndn;
using namespace boost;

class BufferObserver : public IBufferObserver {
    public:
    BufferObserver(boost::shared_ptr<IPipeliner> pipeliner,
                   boost::shared_ptr<IInterestControl> interestControl)
                   : pipeliner_(pipeliner)
                   , interestControl_(interestControl) {}
    ~BufferObserver(){}

    void onNewRequest(const boost::shared_ptr<BufferSlot>&) {}
    void onNewData(const BufferReceipt& receipt)
    {
        if (receipt.oldState_ == BufferSlot::State::New)
        {
            interestControl_->decrement();

            if (receipt.slot_->getNameInfo().class_ == SampleClass::Key)
                pipeliner_->setNeedSample(SampleClass::Key);
        }
    }
    void onReset() {}

    private:
        boost::shared_ptr<IPipeliner> pipeliner_;
        boost::shared_ptr<IInterestControl> interestControl_;
};
#if 0
class PlaybackObserver : public IVideoPlayoutObserver {
    public:
        PlaybackObserver(boost::shared_ptr<IPipeliner> pipeliner,
                         boost::shared_ptr<IInterestControl> interestControl,
                         const Name& threadPrefix) : pipeliner_(pipeliner),
                            interestControl_(interestControl),
                            threadPrefix_(threadPrefix)
                        {}

        void frameSkipped(PacketNumber pNo, bool isKey) override {
            onNextFrameNeeded(pNo, isKey);
        }
        void frameProcessed(PacketNumber pNo, bool isKey) override {
            onNextFrameNeeded(pNo, isKey);
        }
        void recoveryFailure(PacketNumber sampleNo, bool isKey) override { }
        void onQueueEmpty() { }

    private:
        boost::shared_ptr<IPipeliner> pipeliner_;
        boost::shared_ptr<IInterestControl> interestControl_;
        Name threadPrefix_;

        void onNextFrameNeeded(PacketNumber pNo, bool isKey) {
            interestControl_->decrement();
            if (isKey)
                pipeliner_->setNeedSample(SampleClass::Key);
            pipeliner_->fillUpPipeline(threadPrefix_);
        }
};
#endif

RemoteVideoStreamImpl::RemoteVideoStreamImpl(boost::asio::io_service &io,
                                             const boost::shared_ptr<ndn::Face> &face,
                                             const boost::shared_ptr<ndn::KeyChain> &keyChain,
                                             const std::string &streamPrefix)
    : RemoteStreamImpl(io, face, keyChain, streamPrefix)
    , isPlaybackDriven_(false)
{
    construct();
}

RemoteVideoStreamImpl::RemoteVideoStreamImpl(boost::asio::io_service &io,
                                             const boost::shared_ptr<ndn::Face> &face,
                                             const boost::shared_ptr<ndn::KeyChain> &keyChain,
                                             const std::string &streamPrefix,
                                             const std::string &threadName)
    : RemoteStreamImpl(io, face, keyChain, streamPrefix)
    , isPlaybackDriven_(true)
{
    threadName_ = threadName;

    construct();
}

void RemoteVideoStreamImpl::construct()
{
    type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeVideo;

    PipelinerSettings pps;
    pps.interestLifetimeMs_ = 2000;
    pps.sampleEstimator_ = sampleEstimator_;
    pps.buffer_ = buffer_;
    pps.interestControl_ = interestControl_;
    pps.interestQueue_ = interestQueue_;
    pps.playbackQueue_ = playbackQueue_;
    pps.segmentController_ = segmentController_;
    pps.sstorage_ = sstorage_;

    pipeliner_ = make_shared<Pipeliner>(pps, boost::make_shared<Pipeliner::VideoNameScheme>());
    playout_ = boost::make_shared<VideoPlayout>(io_, playbackQueue_, sstorage_);
    playoutControl_ = boost::make_shared<PlayoutControl>(playout_, playbackQueue_, rtxController_);
    playoutControl_->setAdjustQueue(!isPlaybackDriven_);

    playbackQueue_->attach(playoutControl_.get());
    latencyControl_->setPlayoutControl(playoutControl_);
    drdEstimator_->attach(playoutControl_.get());

    validator_ = boost::make_shared<ManifestValidator>(face_, keyChain_, sstorage_);
    buffer_->attach(validator_.get());
}

RemoteVideoStreamImpl::~RemoteVideoStreamImpl()
{
    buffer_->detach(validator_.get());
}

void RemoteVideoStreamImpl::start(const std::string &threadName,
                                  IExternalRenderer *renderer)
{
    if (isPlaybackDriven_)
        throw std::runtime_error("Can't bootstrap for live stream: stream created as playback-driven.");

    assert(renderer);
    renderer_ = renderer;
    RemoteStreamImpl::start(threadName);
}

void RemoteVideoStreamImpl::start(const RemoteVideoStream::FetchingRuleSet& ruleset,
                                  IExternalRenderer *renderer)
{
    if (!isPlaybackDriven_)
        throw std::runtime_error("Trying to initiate playback-driven fetch, but the stream is not "
            "playback-driven.");

    assert(renderer);
    renderer_ = renderer;
    ruleset_ = ruleset;

    sampleEstimator_->bootstrapSegmentNumber(3, SampleClass::Delta, SegmentClass::Data);
    sampleEstimator_->bootstrapSegmentNumber(1, SampleClass::Delta, SegmentClass::Parity);
    sampleEstimator_->bootstrapSegmentNumber(7, SampleClass::Key, SegmentClass::Data);
    sampleEstimator_->bootstrapSegmentNumber(2, SampleClass::Key, SegmentClass::Parity);

    RemoteStreamImpl::start(threadName_);
}

void RemoteVideoStreamImpl::initiateFetching()
{
    RemoteStreamImpl::initiateFetching();

    if (isPlaybackDriven_)
    {
        // playbackObserver_ = boost::make_shared<PlaybackObserver>(pipeliner_,
        //                                                          interestControl_,
        //                                                          getStreamPrefix().append(threadName_));
        // dynamic_pointer_cast<VideoPlayout>(playout_)->attach(playbackObserver_.get());
    }
    else
    {
        bufferObserver_ = boost::make_shared<BufferObserver>(pipeliner_, interestControl_);
        buffer_->attach(bufferObserver_.get());
    }

    setupDecoder();
    setupPipelineControl();
    pipelineControl_->start(threadsMeta_[threadName_]);
}

void RemoteVideoStreamImpl::stopFetching()
{
    RemoteStreamImpl::stopFetching();

    // if (isPlaybackDriven_)
    //     dynamic_pointer_cast<VideoPlayout>(playout_)->detach(playbackObserver_.get());
    // else
    //     buffer_->detach(bufferObserver_.get());

    releasePipelineControl();
    releaseDecoder();
}

void RemoteVideoStreamImpl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    RemoteStreamImpl::setLogger(logger);
    validator_->setLogger(logger);
    boost::dynamic_pointer_cast<NdnRtcComponent>(playoutControl_)->setLogger(logger);
    boost::dynamic_pointer_cast<Playout>(playout_)->setLogger(logger);
}

#pragma mark private
void RemoteVideoStreamImpl::feedFrame(const FrameInfo &frameInfo, const WebRtcVideoFrame &frame)
{
    IExternalRenderer::BufferType bufferType = IExternalRenderer::kARGB;
    uint8_t *rgbFrameBuffer = renderer_->getFrameBuffer(frame.width(),
                                                        frame.height(),
                                                        &bufferType);

    if (rgbFrameBuffer)
    {
        LogTraceC << "passing frame " << frameInfo.playbackNo_ << "p to renderer" << std::endl;

        // @see frame-converter.cpp for explanation, why we flipping ARGB <-> BGRA data representations
        // webrtc::VideoType videoType = (bufferType == IExternalRenderer::kARGB ? webrtc::kARGB : webrtc::kBGRA);
        webrtc::VideoType videoType = (bufferType == IExternalRenderer::kARGB ? webrtc::kBGRA : webrtc::kARGB);

        ConvertFromI420(frame, videoType, 0, rgbFrameBuffer);
        renderer_->renderFrame(frameInfo, frame.width(), frame.height(),
                                   rgbFrameBuffer);
    }
    else
        LogTraceC << "renderer is busy." << std::endl;
}

void RemoteVideoStreamImpl::setupDecoder()
{
    boost::shared_ptr<RemoteVideoStreamImpl> me = boost::dynamic_pointer_cast<RemoteVideoStreamImpl>(shared_from_this());
    VideoThreadMeta meta(threadsMeta_[threadName_]->data());
    // boost::shared_ptr<VideoDecoder> decoder =
    //     boost::make_shared<VideoDecoder>(meta.getCoderParams(),
    //                                      [this, me](const FrameInfo& finfo, const WebRtcVideoFrame &frame)
    //                                      {
    //                                         feedFrame(finfo, frame);
    //                                      });
    // boost::dynamic_pointer_cast<VideoPlayout>(playout_)->registerFrameConsumer(decoder.get());
    // decoder_ = decoder;
}

void RemoteVideoStreamImpl::releaseDecoder()
{
    dynamic_pointer_cast<VideoPlayout>(playout_)->deregisterFrameConsumer();
    decoder_.reset();
}

void RemoteVideoStreamImpl::setupPipelineControl()
{
    Name threadPrefix(getStreamPrefix());
    threadPrefix.append(threadName_);

    if (isPlaybackDriven_)
        pipelineControl_ = boost::make_shared<PipelineControl>(
            PipelineControl::seedPipelineControl(ruleset_,
                                              threadPrefix.toUri(),
                                              drdEstimator_,
                                              boost::dynamic_pointer_cast<IBuffer>(buffer_),
                                              boost::dynamic_pointer_cast<IPipeliner>(pipeliner_),
                                              boost::dynamic_pointer_cast<IInterestControl>(interestControl_),
                                              boost::dynamic_pointer_cast<ILatencyControl>(latencyControl_),
                                              boost::dynamic_pointer_cast<IPlayoutControl>(playoutControl_),
                                              sampleEstimator_,
                                              sstorage_));
    else
        pipelineControl_ = boost::make_shared<PipelineControl>(
            PipelineControl::videoPipelineControl(threadPrefix.toUri(),
                                              drdEstimator_,
                                              boost::dynamic_pointer_cast<IBuffer>(buffer_),
                                              boost::dynamic_pointer_cast<IPipeliner>(pipeliner_),
                                              boost::dynamic_pointer_cast<IInterestControl>(interestControl_),
                                              boost::dynamic_pointer_cast<ILatencyControl>(latencyControl_),
                                              boost::dynamic_pointer_cast<IPlayoutControl>(playoutControl_),
                                              sampleEstimator_,
                                              sstorage_));
    pipelineControl_->setLogger(logger_);
    rtxController_->attach(pipelineControl_.get());
    segmentController_->attach(pipelineControl_.get());
    latencyControl_->registerObserver(pipelineControl_.get());
}

void RemoteVideoStreamImpl::releasePipelineControl()
{
    latencyControl_->unregisterObserver();
    segmentController_->detach(pipelineControl_.get());

    pipelineControl_.reset();
}
