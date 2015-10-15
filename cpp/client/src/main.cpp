// 
// main.cpp
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//
#define FETCH_VIDEO
#define FETCH_AUDIO
//#define PUB_VIDEO

#include <fstream>
#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/ndnrtc-library.h>

#include "config.h"
#include "renderer.h"
#include "NdnRtcLibraryController.h"

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
	#if 1
	if (!configFile)
	{
		std::cout << "usage: " << argv[0] << " -c <config_file>" << std::endl;
		exit(1);
	}
	#endif

	run(configFile);

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
	AppParams headlessParams;

	LogInfo("") << "loading params from " << configFile << "..." << std::endl;
	int flagLoadParamsFromFile = loadParamsFromFile(configFile, headlessParams);
	LogInfo("") << "flagLoadParamsFromFile: " << flagLoadParamsFromFile << std::endl;
	
	const int videoStreamsRendererNumber=getVideoStreamsNumberFromFile(configFile);
	LogInfo("") << "videoStreamsRendererNumber: " << videoStreamsRendererNumber << std::endl;
	RendererInternal renderer[videoStreamsRendererNumber];

	GeneralParams generalParams=headlessParams.generalParams_;
	LogInfo("") << "general configuration:\n" << generalParams << std::endl; 
	GeneralConsumerParams aconsumerParams=headlessParams.audioConsumerParams_;
	LogInfo("") << "aconsumerParams configuration:\n" << aconsumerParams << std::endl; 
	GeneralConsumerParams vconsumerParams=headlessParams.videoConsumerParams_;
	LogInfo("") << "vconsumerParams configuration:\n" << vconsumerParams << std::endl; 

std::string videoStreamPrefix, audioStreamPrefix;
std::string pub_videoStreamPrefix, pub_audioStreamPrefix;

#ifdef FETCH_AUDIO
	{// setup audio fetching
		const int audioStreamsNumber=getAudioStreamsNumberFromFile(configFile);

		for(int audioStreamsCount=0; audioStreamsCount<audioStreamsNumber;audioStreamsCount++){
			MediaStreamParams astreamParams;
			std::string audioStreamSessionPrefix;
			std::string athreadToFetch;
			loadAudioStreamParamsFromFile(configFile, astreamParams, audioStreamSessionPrefix, athreadToFetch, audioStreamsCount);

			const int audioStreamThreadNumber=getAudioStreamThreadNumberFromFile(configFile, audioStreamsCount);

			for (int AudioStreamThreadCount = 0; AudioStreamThreadCount < audioStreamThreadNumber; AudioStreamThreadCount++){
				AudioThreadParams *athreadParams = new AudioThreadParams();
				loadAudioStreamThreadParamsFromFile(configFile, athreadParams, audioStreamsCount, AudioStreamThreadCount);
				astreamParams.mediaThreads_.push_back(athreadParams);
				LogInfo("") << "remote audio stream "<< AudioStreamThreadCount << " configuration:\n" << astreamParams << std::endl;
				athreadParams = NULL;
			}

			LogInfo("") << "initiating remote audio stream for " << audioStreamSessionPrefix << std::endl;
			audioStreamPrefix = ndnp->addRemoteStream(audioStreamSessionPrefix, athreadToFetch, astreamParams, generalParams, aconsumerParams, NULL);
			LogInfo("") << "demo audio fetching " << audioStreamsCount <<" from " << audioStreamPrefix << " initiated..." << std::endl;
		}
		
	}
#endif

#ifdef FETCH_VIDEO
	{// setup video fetching
		const int videoStreamsNumber=getVideoStreamsNumberFromFile(configFile);

		for(int videoStreamsCount=0; videoStreamsCount<videoStreamsNumber;videoStreamsCount++){
			MediaStreamParams vstreamParams;
			std::string videoStreamSessionPrefix;
			std::string vthreadToFetch;
			loadVideoStreamParamsFromFile(configFile, vstreamParams, videoStreamSessionPrefix, vthreadToFetch, videoStreamsCount);

			const int videoStreamThreadNumber=getVideoStreamThreadNumberFromFile(configFile, videoStreamsCount);

			for (int videoStreamThreadCount = 0; videoStreamThreadCount < videoStreamThreadNumber; videoStreamThreadCount++){
				VideoThreadParams *vthreadParams = new VideoThreadParams();
				loadVideoStreamThreadParamsFromFile(configFile, vthreadParams,videoStreamsCount,videoStreamThreadCount);
				vstreamParams.mediaThreads_.push_back(vthreadParams);
				LogInfo("") << "remote video stream "<< videoStreamThreadCount << " configuration:\n" << vstreamParams << std::endl;
				vthreadParams = NULL;
			}

			LogInfo("") << "initiating remote video stream for " << videoStreamSessionPrefix << ", vconsumerParams: " << vconsumerParams << std::endl;
			videoStreamPrefix = ndnp->addRemoteStream(videoStreamSessionPrefix, vthreadToFetch, vstreamParams, generalParams, vconsumerParams, &renderer[videoStreamsCount]);
			LogInfo("") << "demo video fetching " << videoStreamsCount <<" from " << videoStreamPrefix << " initiated..." << std::endl;
		}
		
	}
#endif

#ifdef PUB_VIDEO
	{
		std::string pub_username="ubuntundnrtc";
		std::string pub_sessionPrefix; //= "/ndn/edu/ucla/remap/ndnrtc/user/ubuntundnrtc/streams/camera";
		//SessionObserver *sessionObserverInstance = new SessionObserver();
			//ndnrtc::IExternalCapturer* libcapturer;
		//MediaStreamParams pub_vstreamParams=vstreamParams;
		//pub_sessionPrefix=ndnp->startSession(pub_username,generalParams,sessionObserverInstance);
		//LogInfo("") << "pub_sessionPrefix: " << pub_sessionPrefix << std::endl;
		//pub_videoStreamPrefix=ndnp->addLocalStream(pub_sessionPrefix,pub_vstreamParams, libcapturer);
	}
#endif

	//statistics::StatisticsStorage remoteStreamStatistics_audio=ndnp->getRemoteStreamStatistics(audioStreamPrefix);
	//LogInfo("") << "RemoteStreamStatistics audio: " << remoteStreamStatistics_audio << std::endl;
	//std::ofstream myfile;
	//myfile.open ("example.txt");
	//myfile << RemoteStreamStatistics_audio;
	

	sleep(20);

	ndnp->removeRemoteStream(videoStreamPrefix);
	ndnp->removeRemoteStream(audioStreamPrefix);
	//myfile.close();
	LogInfo("") << "demo fetching has been completed" << std::endl;

	return;
}


