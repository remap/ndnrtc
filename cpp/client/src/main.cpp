//
// main.cpp
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//

#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <boost/asio.hpp>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/interfaces.h>

#include "config.h"
#include "renderer.h"
#include "statistics.h"
#include "capturer.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;

void removeRemoteStreams(INdnRtcLibrary *ndnp, std::vector<std::string> &StreamsPrefix);

void run(const std::string &configFile,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSecconst,
         const unsigned int statisticsSampleInterval);

//******************************************************************************
int main(int argc, char **argv) 
{
    char *configFile = NULL;
    int c;
    unsigned int runTimeSec = 20; // default app run time (sec)
    unsigned int statSamplePeriodMs = 10;  // default statistics sample interval (ms)
    ndnlog::NdnLoggerDetailLevel logLevel = ndnlog::NdnLoggerDetailLevelDefault;

    opterr = 0;
    while ((c = getopt (argc, argv, "vi:t:c:")) != -1)
        switch (c) {
        case 'c':
            configFile = optarg;
            break;
        case 'v':
            logLevel = ndnlog::NdnLoggerDetailLevelAll;
            break;
        case 'i':
            statSamplePeriodMs = (unsigned int)atoi(optarg);
            break;
        case 't':
            runTimeSec = (unsigned int)atoi(optarg);
            break;
        case '?':
            if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr,
                         "Unknown option character `\\x%x'.\n",
                         optopt);
            return 1;
        default:
            abort ();
        }

    if (!configFile) 
    {
        std::cout << "usage: " << argv[0] << " -c <config_file> -t <app run time in seconds> -i "
        "<statistics sample interval in milliseconds> [-v <verbose mode>]" << std::endl;
        exit(1);
    }

    run(configFile, logLevel, runTimeSec, statSamplePeriodMs);

    return 0;
}

//******************************************************************************
class LibraryObserver : public INdnRtcLibraryObserver {
  public:
    void onStateChanged(const char *state, const char *args) 
    {
        LogInfo("") << "library state changed: " << state << "-" << args << std::endl;
    }

    void onErrorOccurred(int errorCode, const char *message) 
    {
        LogError("") << "library returned error (" << errorCode << ") " << message << std::endl;
    }
};

static INdnRtcLibrary *ndnp = NULL;
static LibraryObserver libObserver;

//******************************************************************************
void run(const std::string &configFile, const ndnlog::NdnLoggerDetailLevel logLevel, 
    const unsigned int runTimeSec, const unsigned int statSamplePeriodMs) 
{
    ndnp = &(NdnRtcLibrary::getSharedInstance());

    ClientParams headlessParams;
    std::vector<std::string> remoteStreamsPrefix;

    ndnlog::new_api::Logger::getLogger("").setLogLevel(logLevel);

    LogInfo("") << "Run time is set to " << runTimeSec << " seconds, loading "
    "params from " << configFile << "..." << std::endl;

    if (loadParamsFromFile(configFile, headlessParams) == EXIT_FAILURE) 
    {
        LogError("") << "loading params from " << configFile << " met error!" << std::endl;
        return;
    }

    LogInfo("") << "Parameters loaded:\n" << headlessParams << std::endl;
    LogDebug("") << "general configuration:\n" << headlessParams.generalParams_ << std::endl;
    LogDebug("") << "audioConsumerParams configuration:\n" 
        << headlessParams.audioConsumerParams_ << std::endl;
    LogDebug("") << "videoConsumerParams configuration:\n" 
        << headlessParams.videoConsumerParams_ << std::endl;

#if 0
    // setup audio fetching
    const int audioStreamsNumber = headlessParams.defaultAudioStreams_.size();

    for (int audioStreamsCount = 0; audioStreamsCount < audioStreamsNumber; audioStreamsCount++) {
        MediaStreamParamsSupplement *audioStream = headlessParams.getMediaStream(headlessParams.defaultAudioStreams_, audioStreamsCount);
        LogDebug("") << "initiating remote audio stream for "
                     << audioStream->streamPrefix_ << std::endl;

        std::string audioStreamPrefix = ndnp->addRemoteStream(audioStream->streamPrefix_ , audioStream->threadToFetch_, (*audioStream), headlessParams.generalParams_, headlessParams.audioConsumerParams_, NULL);
        remoteStreamsPrefix.push_back(audioStreamPrefix);
        LogInfo("") << "demo audio fetching " << audioStreamsCount << " from " << audioStreamPrefix << " initiated, "
                    << "threadToFetch: " << audioStream->threadToFetch_ << std::endl;
    }

    // setup video fetching
    const int videoStreamsNumber = headlessParams.defaultVideoStreams_.size();

    LogDebug("") << "videoStreamsNumber: " << videoStreamsNumber << std::endl;
    RendererInternal *renderer = new RendererInternal[videoStreamsNumber];

    for (int videoStreamsCount = 0; videoStreamsCount < videoStreamsNumber; videoStreamsCount++) {
        MediaStreamParamsSupplement *videoStream = headlessParams.getMediaStream(headlessParams.defaultVideoStreams_, videoStreamsCount);

        LogDebug("") << "initiating remote video stream for " << videoStream->streamPrefix_ << ", vconsumerParams: " << headlessParams.videoConsumerParams_ << std::endl;
        std::string videoStreamPrefix = ndnp->addRemoteStream(videoStream->streamPrefix_, videoStream->threadToFetch_, (*videoStream), headlessParams.generalParams_, headlessParams.videoConsumerParams_, &renderer[videoStreamsCount]);
        remoteStreamsPrefix.push_back(videoStreamPrefix);
        LogInfo("") << "demo video fetching " << videoStreamsCount << " from " << videoStreamPrefix << " initiated, "
                    << "threadToFetch: " << videoStream->threadToFetch_ << std::endl;
    }

    // local_session_prefix = "/ndn/edu/ucla/remap/ndnrtc/user/ubuntuHeadless";
    // new_api::MediaStreamParams localMedia;
    // localMedia.type_=MediaStreamParams::MediaStreamTypeAudio;
    // localMedia.producerParams_.segmentSize_ = 1000;
    // localMedia.producerParams_.freshnessMs_ = 1000;
    // localMedia.streamName_ = "audio";
    // localMedia.synchronizedStreamName_ = "audio";
    //         // CaptureDeviceParams *captureDevice_ = NULL;
    // MediaThreadParams localMediaThread;
    // localMediaThread.threadName_="pcmu";
    // localMedia.mediaThreads_.push_back(&localMediaThread);
    // SessionObserver *localSessionObserver=new SessionObserver;
    // GeneralParams localMediaGeneralParams=headlessParams.generalParams_;
    // localMediaGeneralParams.prefix_="/ndn/edu/ucla";
    // IExternalCapturer** localCapturer=NULL;

    // std::string localStreamsPrefixSession=ndnp->startSession("ubuntuHeadless",
    //                                                     localMediaGeneralParams,
    //                                                     localSessionObserver);
    // std::string localStreamsPrefix=ndnp->addLocalStream(localStreamsPrefixSession,
    //                                                 localMedia,
    //                                                 NULL);
    // LogDebug("")<< "localStreamsPrefix: " << localStreamsPrefix<<std::endl;

    new_api::MediaStreamParams localMedia;

    localMedia.type_ = MediaStreamParams::MediaStreamTypeVideo;
    localMedia.producerParams_.segmentSize_ = 1000;
    localMedia.producerParams_.freshnessMs_ = 1000;
    localMedia.streamName_ = "video";
    localMedia.synchronizedStreamName_ = "video";
    // CaptureDeviceParams *captureDevice_ = NULL;
    // MediaThreadParams* localMediaThread = new VideoThreadParams;
    VideoThreadParams *localMediaThread = new VideoThreadParams;

    localMediaThread->threadName_ = "freec";
    localMediaThread->deltaAvgSegNum_ = 5;
    localMediaThread->deltaAvgParitySegNum_ = 2;
    localMediaThread->keyAvgSegNum_ = 30;
    localMediaThread->keyAvgParitySegNum_ = 5;
    localMediaThread->coderParams_.codecFrameRate_ = 30;
    localMediaThread->coderParams_.gop_ = 30;
    localMediaThread->coderParams_.startBitrate_ = 100;
    localMediaThread->coderParams_.maxBitrate_ = 10000;
    localMediaThread->coderParams_.encodeWidth_ = 720;
    localMediaThread->coderParams_.encodeHeight_ = 405;
    localMediaThread->coderParams_.dropFramesOn_ = true;
    localMedia.mediaThreads_.push_back(localMediaThread);

    SessionObserver *localSessionObserver = new SessionObserver;
    GeneralParams localMediaGeneralParams = headlessParams.generalParams_;

    localMediaGeneralParams.prefix_ = "/a";
    // localMediaGeneralParams.prefix_ = "/ndn/edu/ucla";

    IExternalCapturer *localCapturer;

    std::string localStreamsPrefixSession = ndnp->startSession("ubuntuHeadless",
                                            localMediaGeneralParams,
                                            localSessionObserver);
    std::string localStreamsPrefix = ndnp->addLocalStream(localStreamsPrefixSession,
                                     localMedia,
                                     &localCapturer);
    callReaderVideoFromFile(&localCapturer, headlessAppOnlineTimeSec);
    LogDebug("") << "localStreamsPrefix: " << localStreamsPrefix << std::endl;

    // callStatCollector(statisticsSampleInterval,
    //                   headlessAppOnlineTimeSec,
    //                   headlessParams.statistics_,
    //                   remoteStreamsPrefix,
    //                   ndnp);

    sleep(headlessAppOnlineTimeSec);
    removeRemoteStreams(ndnp, remoteStreamsPrefix);
    LogDebug("") << "remove remote streams... "  << std::endl;
    // delete  []renderer;
    LogDebug("") << "delete renderers... " << std::endl;
    LogInfo("") << "demo fetching has been completed" << std::endl;
#endif
    return;
}
void removeRemoteStreams(INdnRtcLibrary *ndnp, std::vector<std::string> &StreamsPrefix) {

    int streamsPrefixNumber = StreamsPrefix.size();

    for (int streamsPrefixCount = 0; streamsPrefixCount < streamsPrefixNumber; streamsPrefixCount++) {
        ndnp->removeRemoteStream(StreamsPrefix[streamsPrefixCount]);
    }
    return;

}

