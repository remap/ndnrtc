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
#include <ndnrtc/ndnrtc-library.h>

#include "config.h"
#include "renderer.h"
#include "statistics.h"
#include "capturer.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;

void removeRemoteStreams(NdnRtcLibrary *&ndnp, std::vector<std::string> &StreamsPrefix);

void run(const std::string &configFile,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSecconst,
         const unsigned int statisticsSampleInterval);

int main(int argc, char **argv) {
    char *configFile = NULL;
    int index;
    int c;
    unsigned int headlessAppOnlineTimeSec = 20; //default app online time
    unsigned int statisticsSampleInterval = 1; //default statistics sample interval
    ndnlog::NdnLoggerDetailLevel appLoggingLevel = ndnlog::NdnLoggerDetailLevelDefault;

    opterr = 0;
    while ((c = getopt (argc, argv, "vi:t:c:")) != -1)
        switch (c) {
        case 'c':
            configFile = optarg;
            break;
        case 'v':
            appLoggingLevel = ndnlog::NdnLoggerDetailLevelAll;
            break;
        case 'i':
            statisticsSampleInterval = (unsigned int)atoi(optarg);
            break;
        case 't':
            headlessAppOnlineTimeSec = (unsigned int)atoi(optarg);
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

// #warning implement loading parameters from configuration files
#if 1
    if (!configFile) {

        std::cout << "usage: " << argv[0] << " -c <config_file> -t <app online time in seconds> -i <statistics sample interval in seconds> -v <verbose mode>" << std::endl;
        exit(1);
    }
#endif

    run(configFile, appLoggingLevel, headlessAppOnlineTimeSec, statisticsSampleInterval);

    return 0;
}

//******************************************************************************
class LibraryObserver : public INdnRtcLibraryObserver {
public:
    void onStateChanged(const char *state, const char *args) {

        LogInfo("") << "library state changed: " << state << "-" << args << std::endl;
    }

    void onErrorOccurred(int errorCode, const char *message) {

        LogError("") << "library returned error (" << errorCode << ") " << message << std::endl;
    }
};

static NdnRtcLibrary *ndnp = NULL;
static LibraryObserver libObserver;

//******************************************************************************
void run(const std::string &configFile,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSec,
         const unsigned int statisticsSampleInterval) {
    ndnp = &(NdnRtcLibrary::getSharedInstance());
    ClientParams headlessParams;
    std::vector<std::string> remoteStreamsPrefix;

    ndnlog::new_api::Logger::getLogger("").setLogLevel(appLoggingLevel);
    LogInfo("") << "app online time is set to " << headlessAppOnlineTimeSec << " seconds, loading params from " << configFile << "..." << std::endl;

    if (loadParamsFromFile(configFile, headlessParams) == EXIT_FAILURE) {
        LogError("") << "loading params from " << configFile << " met error!" << std::endl;
        return;
    }

    LogInfo("") << "All headlessParams:" << headlessParams << std::endl;
    LogDebug("") << "general configuration:\n" << headlessParams.generalParams_ << std::endl;
    LogDebug("") << "audioConsumerParams configuration:\n" << headlessParams.audioConsumerParams_ << std::endl;
    LogDebug("") << "videoConsumerParams configuration:\n" << headlessParams.videoConsumerParams_ << std::endl;

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
    new_api::MediaStreamParams localMedia;
    localMedia.type_=MediaStreamParams::MediaStreamTypeAudio;
    localMedia.producerParams_.segmentSize_ = 1000;
    localMedia.producerParams_.freshnessMs_ = 1000;
    localMedia.streamName_ = "audio";
    localMedia.synchronizedStreamName_ = "audio";
            // CaptureDeviceParams *captureDevice_ = NULL;
    MediaThreadParams localMediaThread;
    localMediaThread.threadName_="pcmu";
    localMedia.mediaThreads_.push_back(&localMediaThread);
    SessionObserver *localSessionObserver=new SessionObserver;
    GeneralParams localMediaGeneralParams=headlessParams.generalParams_;
    localMediaGeneralParams.prefix_="/ndn/edu/ucla";
    IExternalCapturer* localCapturer=new ExternalCapturer;

    std::string localStreamsPrefixSession=ndnp->startSession("ubuntuHeadless",
                                                        localMediaGeneralParams,
                                                        localSessionObserver);
    std::string localStreamsPrefix=ndnp->addLocalStream(localStreamsPrefixSession,
                                                    localMedia,
                                                    NULL);
    LogDebug("")<< "localStreamsPrefix: " << localStreamsPrefix<<std::endl;

    // collect streams statictics
    boost::asio::io_service staticticsIo;
    LogDebug("") << "statisticsSampleInterval(s): " << statisticsSampleInterval << std::endl;
    collectStreamsStatictics collectStatictics(staticticsIo, headlessAppOnlineTimeSec, statisticsSampleInterval, headlessParams.statistics_, remoteStreamsPrefix, ndnp);
    staticticsIo.run();
    staticticsIo.stop();

    // sleep(headlessAppOnlineTimeSec);
    // removeRemoteStreams(ndnp, remoteStreamsPrefix);
    LogDebug("") << "remove remote streams... "  << std::endl;
    // delete  []renderer;
    // LogDebug("") << "delete renderers... " << std::endl;
    LogInfo("") << "demo fetching has been completed" << std::endl;

    return;
}
void removeRemoteStreams(NdnRtcLibrary *&ndnp, std::vector<std::string> &StreamsPrefix) {

    int streamsPrefixNumber = StreamsPrefix.size();

    for (int streamsPrefixCount = 0; streamsPrefixCount < streamsPrefixNumber; streamsPrefixCount++) {
        ndnp->removeRemoteStream(StreamsPrefix[streamsPrefixCount]);
    }
    return;

}

