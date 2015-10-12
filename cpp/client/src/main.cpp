// 
// main.cpp
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//
#define FETCH_VIDEO
#define FETCH_AUDIO
//#define FETCH_VIDEO_AND_AUDIO

#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/ndnrtc-library.h>

#include "renderer.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;

void run(const std::string& configFile);

int main(int argc, char **argv)
{
	char *configFile = NULL;
	int index;
	int c;

	opterr = 0;
	while ((c = getopt (argc, argv, "c:")) != -1)
		switch (c)
		{
			case 'c':
				configFile = optarg;
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

	// remove this for now
#warning implement loading parameters from configuration files
	#if 0
	if (!configFile)
	{
		std::cout << "usage: " << argv[0] << " -c <config_file>" << std::endl;
		exit(1);
	}
	#endif

	run("");

	return 0;
}

//******************************************************************************
class LibraryObserver : public INdnRtcLibraryObserver
{
public:
    void onStateChanged(const char *state, const char *args)
    {
        LogInfo("") << "library state changed: " << state << "-" << args << std::endl;
    }
    
    void onErrorOccurred(int errorCode, const char* message)
    {
       LogError("") << "library returned error (" << errorCode << ") " << message << std::endl;
    }
};

static NdnRtcLibrary* ndnp = NULL;
static LibraryObserver libObserver;

//******************************************************************************
void run(const std::string& configFile)
{
	ndnp = &(NdnRtcLibrary::getSharedInstance());

	LogInfo("") << "loading params from " << configFile << "..." << std::endl;
	
	std::string sessionPrefix = "/ndn/edu/byu/ndnrtc/user/freeculture";
	GeneralParams generalParams;
	{
		// to save performance on VM
		generalParams.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
		generalParams.logFile_ = "ndnrtc.log";
		generalParams.logPath_ = "/tmp";
		generalParams.useTlv_ = true;
		generalParams.useRtx_ = true;
		generalParams.useFec_ = true;
		generalParams.useAudio_ = true;
		generalParams.useVideo_ = true;
		generalParams.useAvSync_ = true;
		generalParams.skipIncomplete_ = true;

		// out prefix in case of publishing
		generalParams.prefix_ = "/ndn/edu/ucla/remap";
		// NFD host
		generalParams.host_ = "localhost";

		LogInfo("") << "general configuration:\n" << generalParams << std::endl; 
	}

	GeneralConsumerParams consumerParams;
	{
		consumerParams.interestLifetime_ = 2000;
		consumerParams.bufferSlotsNum_ = 200;
		consumerParams.slotSize_ = 64000;
		consumerParams.jitterSizeMs_ = 300;

		LogInfo("") << "fetching configuration:\n" << consumerParams << std::endl;
	}

std::string videoStreamPrefix, audioStreamPrefix;
#ifdef FETCH_AUDIO
	{// setup audio fetching
		MediaStreamParams astreamParams;
		//MediaStreamParams streamParams;
		{
			astreamParams.streamName_ = "sound";
			astreamParams.type_ = MediaStreamParams::MediaStreamTypeAudio;
			astreamParams.producerParams_.segmentSize_ = 1000;
			astreamParams.producerParams_.freshnessMs_ = 0; // doesn't matter for fetching

			// thread params we want to fetch from
			AudioThreadParams *athreadParams = new AudioThreadParams();
			athreadParams->threadName_ = "pcmu";

			astreamParams.mediaThreads_.push_back(athreadParams);
			LogInfo("") << "remote stream configuration:\n" << astreamParams << std::endl;
		}

		//std::string threadToFetch = "pcmu";
		std::string athreadToFetch;
		athreadToFetch = "pcmu";
	
		LogInfo("") << "initiating remote audio stream for " << sessionPrefix << std::endl;
		audioStreamPrefix = ndnp->addRemoteStream(sessionPrefix, athreadToFetch, astreamParams, generalParams, consumerParams, NULL);
		LogInfo("") << "demo audio fetching from " << audioStreamPrefix << " initiated..." << std::endl;
	}
#endif

#ifdef FETCH_VIDEO
	{// setup video fetching
		MediaStreamParams vstreamParams;
		//MediaStreamParams streamParams;
		{
			vstreamParams.streamName_ = "movie";
			vstreamParams.type_ = MediaStreamParams::MediaStreamTypeVideo;
			vstreamParams.producerParams_.segmentSize_ = 1000;
			vstreamParams.producerParams_.freshnessMs_ = 0; // doesn't matter for fetching
	
			// thread params we want to fetch from
			VideoThreadParams *vthreadParams = new VideoThreadParams();
			vthreadParams->threadName_ = "mid";
			vthreadParams->coderParams_.codecFrameRate_ = 30;
			vthreadParams->coderParams_.gop_ = 30;
			vthreadParams->coderParams_.startBitrate_ = 1000;
			vthreadParams->coderParams_.maxBitrate_ = 10000;
			vthreadParams->coderParams_.encodeHeight_ = 720;
			vthreadParams->coderParams_.encodeWidth_ = 1280;
			vthreadParams->deltaAvgSegNum_ = 5;
			vthreadParams->deltaAvgParitySegNum_ = 2;
			vthreadParams->keyAvgSegNum_ = 30;
			vthreadParams->keyAvgParitySegNum_ = 5;
	
			vstreamParams.mediaThreads_.push_back(vthreadParams);
			LogInfo("") << "remote video stream configuration:\n" << vstreamParams << std::endl;
		}
	
		std::string vthreadToFetch;
		//std::string threadToFetch = "mid";
		vthreadToFetch = "mid";
		RendererInternal renderer;
	
		LogInfo("") << "initiating remote video stream for " << sessionPrefix << std::endl;
		videoStreamPrefix = ndnp->addRemoteStream(sessionPrefix, vthreadToFetch, vstreamParams, generalParams, consumerParams, &renderer);
		LogInfo("") << "demo video fetching from " << videoStreamPrefix << " initiated..." << std::endl;
	}
#endif

#ifdef FETCH_VIDEO_AND_AUDIO
	{// setup audio fetching
		MediaStreamParams streamParamsa;
		{
			streamParamsa.streamName_ = "sound";
			streamParamsa.type_ = MediaStreamParams::MediaStreamTypeAudio;
			streamParamsa.producerParams_.segmentSize_ = 1000;
			streamParamsa.producerParams_.freshnessMs_ = 0; // doesn't matter for fetching

			// thread params we want to fetch from
			AudioThreadParams *threadParams = new AudioThreadParams();
			threadParams->threadName_ = "pcmu";

			streamParamsa.mediaThreads_.push_back(threadParams);
			LogInfo("") << "remote stream configuration:\n" << streamParamsa << std::endl;
		}

		std::string threadToFetcha = "pcmu";
	
		LogInfo("") << "initiating remote audio stream for " << sessionPrefix << std::endl;
		audioStreamPrefix = ndnp->addRemoteStream(sessionPrefix, threadToFetcha, streamParamsa, generalParams, consumerParams, NULL);
		LogInfo("") << "demo audio fetching from " << audioStreamPrefix << " initiated..." << std::endl;
	
		MediaStreamParams streamParamsv;
		{
			streamParamsv.streamName_ = "movie";
			streamParamsv.type_ = MediaStreamParams::MediaStreamTypeVideo;
			streamParamsv.producerParams_.segmentSize_ = 1000;
			streamParamsv.producerParams_.freshnessMs_ = 0; // doesn't matter for fetching
	
			// thread params we want to fetch from
			VideoThreadParams *threadParams = new VideoThreadParams();
			threadParams->threadName_ = "mid";
			threadParams->coderParams_.codecFrameRate_ = 30;
			threadParams->coderParams_.gop_ = 30;
			threadParams->coderParams_.startBitrate_ = 1000;
			threadParams->coderParams_.maxBitrate_ = 10000;
			threadParams->coderParams_.encodeHeight_ = 720;
			threadParams->coderParams_.encodeWidth_ = 1280;
			threadParams->deltaAvgSegNum_ = 5;
			threadParams->deltaAvgParitySegNum_ = 2;
			threadParams->keyAvgSegNum_ = 30;
			threadParams->keyAvgParitySegNum_ = 5;
	
			streamParamsv.mediaThreads_.push_back(threadParams);
			LogInfo("") << "remote video stream configuration:\n" << streamParamsv << std::endl;
		}
	
		
		std::string threadToFetchv = "mid";
		RendererInternal renderer;
	
		LogInfo("") << "initiating remote video stream for " << sessionPrefix << std::endl;
		videoStreamPrefix = ndnp->addRemoteStream(sessionPrefix, threadToFetchv, streamParamsv, generalParams, consumerParams, &renderer);
		LogInfo("") << "demo video fetching from " << videoStreamPrefix << " initiated..." << std::endl;
}
#endif

	sleep(30);

	ndnp->removeRemoteStream(videoStreamPrefix);
	ndnp->removeRemoteStream(audioStreamPrefix);
	LogInfo("") << "demo fetching has been completed" << std::endl;

	return;
}


