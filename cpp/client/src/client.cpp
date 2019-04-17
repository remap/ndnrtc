//
// client.cpp
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <algorithm>
#include <memory>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>

#include "client.hpp"
#include "config.hpp"
#include "renderer.hpp"

using namespace std;
using namespace ndn;

//******************************************************************************
void Client::run(unsigned int runTimeSec, unsigned int statSamplePeriodMs,
                 const ClientParams &params, const std::string &instanceName)
{
    runTimeSec_ = runTimeSec;
    statSampleIntervalMs_ = statSamplePeriodMs;
    params_ = params;
    instanceName_ = instanceName;

    bool run = setupConsumer();
    run |= setupProducer();

    if (!run)
        return;

    setupStatGathering();
    runProcessLoop();
    tearDownStatGathering();
    tearDownProducer();
    tearDownConsumer();
}

//******************************************************************************
bool Client::setupConsumer()
{
    if (!params_.isConsuming())
        return false;

    ConsumerClientParams ccp = params_.getConsumerParams();

    for (auto p : ccp.fetchedStreams_)
    {
        ndnrtc::GeneralConsumerParams gp = (p.type_ == ClientMediaStreamParams::MediaStreamType::MediaStreamTypeAudio ? ccp.generalAudioParams_ : ccp.generalVideoParams_);
        RemoteStream rs = initRemoteStream(p, gp);
#warning check move semantics
        remoteStreams_.push_back(boost::move(rs));

        LogInfo("") << "Set up fetching from " << p.sessionPrefix_ << ":"
                    << p.streamName_ << endl;
    }

    LogInfo("") << "Fetching " << remoteStreams_.size() << " remote stream(s) total" << endl;

    return true;
}

bool Client::setupProducer()
{
    if (!params_.isProducing())
        return false;

    ProducerClientParams pcp = params_.getProducerParams();

    for (auto p : pcp.publishedStreams_)
    {
        try
        {
            LocalStream ls = initLocalStream(p);
            localStreams_.push_back(boost::move(ls));

            LogInfo("") << "Set up publishing stream " << pcp.prefix_ << ":"
                        << p.streamName_ << endl;
        }
        catch (const runtime_error &e)
        {
            LogError("") << "error while trying to publish stream " << p.streamName_ << ": "
                         << e.what() << endl;
            throw;
        }
    }

    LogInfo("") << "Publishing " << localStreams_.size() << " stream(s) total" << endl;
    return true;
}

void Client::setupStatGathering()
{
    if (!params_.isGatheringStats())
        return;

    statCollector_.reset(new StatCollector(io_));

    for (auto &rs : remoteStreams_)
        statCollector_->addStream(rs.getStream(), params_.getGeneralParameters().logPath_,
                                  params_.getConsumerParams().statGatheringParams_);
    for (auto &ls : localStreams_)
        statCollector_->addStream(ls.getStream(), params_.getGeneralParameters().logPath_,
                                  params_.getProducerParams().statGatheringParams_);

    statCollector_->startCollecting(statSampleIntervalMs_);

    LogInfo("") << "Gathering statistics into "
                << statCollector_->getWritersNumber() << " files" << std::endl;
}

void Client::runProcessLoop()
{
    boost::asio::deadline_timer runTimer(io_);
    runTimer.expires_from_now(boost::posix_time::seconds(runTimeSec_));
    runTimer.wait();
}

void Client::tearDownStatGathering()
{
    if (!params_.isGatheringStats())
        return;

    statCollector_->stop();
    statCollector_.reset();
    LogInfo("") << "Stopped gathering statistics" << std::endl;
}

void Client::tearDownProducer()
{
    if (!params_.isProducing())
        return;

    LogInfo("") << "Tearing down producing..." << std::endl;

    for (auto &ls : localStreams_)
    {
        if (!ls.getVideoSource().get())
            std::dynamic_pointer_cast<ndnrtc::LocalAudioStream>(ls.getStream())->stop();
        else
            ls.stopSource();

        LogInfo("") << "...stopped publishing " << ls.getStream()->getPrefix() << std::endl;
    }
    localStreams_.clear();
}

void Client::tearDownConsumer()
{
    if (!params_.isConsuming())
        return;

    LogInfo("") << "Tearing down consuming..." << std::endl;

    for (auto &rs : remoteStreams_)
    {
        std::dynamic_pointer_cast<ndnrtc::RemoteStream>(rs.getStream())->stop();
        LogInfo("") << "...stopped fetching from " << rs.getStream()->getPrefix() << std::endl;
    }
    remoteStreams_.clear();
}

RemoteStream Client::initRemoteStream(const ConsumerStreamParams &p,
                                      const ndnrtc::GeneralConsumerParams &gcp)
{
    RendererInternal *renderer = setupRenderer(p);

    if (p.type_ == ConsumerStreamParams::MediaStreamTypeVideo)
    {
        std::shared_ptr<ndnrtc::RemoteVideoStream>
            remoteStream(std::make_shared<ndnrtc::RemoteVideoStream>(io_, face_, keyChain_,
                                                                       p.sessionPrefix_, p.streamName_, gcp.interestLifetime_, gcp.jitterSizeMs_));
        remoteStream->setLogger(consumerLogger(p.sessionPrefix_, p.streamName_));
        remoteStream->start(p.threadToFetch_, renderer);
        return RemoteStream(remoteStream, std::shared_ptr<RendererInternal>(renderer));
    }
    else
    {
        std::shared_ptr<ndnrtc::RemoteAudioStream>
            remoteStream(std::make_shared<ndnrtc::RemoteAudioStream>(io_, face_, keyChain_,
                                                                       p.sessionPrefix_, p.streamName_, gcp.interestLifetime_, gcp.jitterSizeMs_));
        remoteStream->setLogger(consumerLogger(p.sessionPrefix_, p.streamName_));
        remoteStream->start(p.threadToFetch_);
        return RemoteStream(remoteStream, std::shared_ptr<RendererInternal>(renderer));
    }
}

LocalStream Client::initLocalStream(const ProducerStreamParams &p)
{
    std::shared_ptr<ndnrtc::IStream> localStream;
    std::shared_ptr<VideoSource> videoSource;
    ndnrtc::MediaStreamSettings settings(io_, p);

    settings.keyChain_ = keyChain_.get();
    settings.face_ = face_.get();

    if (p.type_ == ndnrtc::MediaStreamParams::MediaStreamTypeVideo)
    {
        LogDebug("") << "initializing video source at " << p.source_.name_ << endl;

        std::shared_ptr<RawFrame> sampleFrame = sampleFrameForStream(p);

        LogDebug("") << "source should support frames of size "
                     << sampleFrame->getWidth() << "x" << sampleFrame->getHeight() << endl;

        std::shared_ptr<IFrameSource> source;

        if (p.source_.type_ == "file")
        {
            if (!FileFrameSource::checkSourceForFrame(p.source_.name_, *sampleFrame))
            {
                stringstream msg;
                msg << "bad source (" << p.source_.name_ << ") for "
                    << sampleFrame->getWidth() << "x" << sampleFrame->getHeight() << " video";
                throw runtime_error(msg.str());
            }
            source.reset(new FileFrameSource(p.source_.name_));
        }
        else if (p.source_.type_ == "pipe")
        {
            source.reset(new PipeFrameSource(p.source_.name_));
        }
        else
            throw runtime_error("Uknown source type "+p.source_.type_);

        videoSource.reset(new VideoSource(io_, source, sampleFrame));
        LogDebug("") << "video source initialized" << endl;

        std::shared_ptr<ndnrtc::LocalVideoStream> s =
            std::make_shared<ndnrtc::LocalVideoStream>(p.sessionPrefix_, settings);

        s->setLogger(producerLogger(p.streamName_));
        videoSource->addCapturer(s.get());
        localStream = s;

#warning double-check whether FPS should be 30
        LogDebug("") << "starting video source..." << endl;
        videoSource->start(30);
        LogDebug("") << "...video source started" << endl;
    }
    else
    {
        std::shared_ptr<ndnrtc::LocalAudioStream> s =
            std::make_shared<ndnrtc::LocalAudioStream>(p.sessionPrefix_, settings);
        s->setLogger(producerLogger(p.streamName_));
        s->start();
        localStream = s;
    }

    return LocalStream(localStream, videoSource);
}

std::shared_ptr<RawFrame> Client::sampleFrameForStream(const ProducerStreamParams &p)
{
    unsigned int width = 0, height = 0;
    p.getMaxResolution(width, height);

    if (width == 0 || height == 0)
    {
        stringstream ss;
        ss << "incorrect max resolution for stream " << p.streamName_;
        throw runtime_error(ss.str());
    }

    return std::shared_ptr<RawFrame>(new ArgbFrame(width, height));
}

std::shared_ptr<ndnlog::new_api::Logger>
Client::producerLogger(std::string streamName)
{
    std::stringstream logFileName;
    logFileName << params_.getGeneralParameters().logPath_ << "/"
                << "producer-" << instanceName_ << "-" << streamName << ".log";
    std::shared_ptr<ndnlog::new_api::Logger> logger(new ndnlog::new_api::Logger(params_.getGeneralParameters().loggingLevel_,
                                                                                  logFileName.str()));

    return logger;
}

std::shared_ptr<ndnlog::new_api::Logger>
Client::consumerLogger(std::string prefix, std::string streamName)
{
    std::replace(prefix.begin(), prefix.end(), '/', '-');
    std::stringstream logFileName;
    logFileName << params_.getGeneralParameters().logPath_ << "/"
                << "consumer" << prefix << "-" << streamName << ".log";
    std::shared_ptr<ndnlog::new_api::Logger> logger(new ndnlog::new_api::Logger(params_.getGeneralParameters().loggingLevel_,
                                                                                  logFileName.str()));

    return logger;
}

RendererInternal *Client::setupRenderer(const ConsumerStreamParams &p)
{
    if (p.type_ == ConsumerStreamParams::MediaStreamTypeVideo)
    {
        if (p.sink_.type_ == "pipe")
            return new RendererInternal(p.sink_.name_,
                                        [p](const std::string &s) -> std::shared_ptr<IFrameSink> {
                                            std::shared_ptr<IFrameSink> sink = std::make_shared<PipeSink>(s);
                                            if (p.sink_.writeFrameInfo_) sink->setWriteFrameInfo(true);
                                            return sink;
                                        }, rendererIo_);
        else if (p.sink_.type_ == "nano")
        {
#ifdef HAVE_LIBNANOMSG
            return new RendererInternal(p.sink_.name_,
                                        [p](const std::string &s) -> std::shared_ptr<IFrameSink> {
                                            try {
                                                std::shared_ptr<IFrameSink> sink = std::make_shared<NanoMsgSink>(s);
                                                if (p.sink_.writeFrameInfo_) sink->setWriteFrameInfo(true);
                                                return sink;
                                            }
                                            catch (std::exception& e)
                                            {
                                                LogError("") << "Error when creating nanomsg sink: " << e.what() << std::endl;
                                                throw;
                                            }
                                        }, rendererIo_);
#else
            throw std::runtime_error("Requested nano type sink, but code was not built with nanomsg library support");
#endif
        }
        else
            return new RendererInternal(p.sink_.name_,
                                        [p](const std::string &s) -> std::shared_ptr<IFrameSink> {
                                            std::shared_ptr<IFrameSink> sink = std::make_shared<FileSink>(s);
                                            if (p.sink_.writeFrameInfo_) sink->setWriteFrameInfo(true);
                                            return sink;
                                        }, rendererIo_);
    }
    else
        return nullptr;
}
