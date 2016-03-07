// 
// main.cpp
//
//  Created by Peter Gusev on 03 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/interfaces.h>

#include "config.h"
#include "renderer.h"
#include "statistics.h"
#include "capturer.h"
#include "client.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

void removeRemoteStreams(INdnRtcLibrary *ndnp, std::vector<std::string> &StreamsPrefix);

void run(const std::string &configFile,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSecconst,
         const unsigned int statisticsSampleInterval);
void setupConsumer(unsigned int runTimeSec, const ClientParams &params);
void setupProducer(unsigned int runTimeSec, const ClientParams &params);
void setupStatGathering(unsigned int runTimeSec, unsigned int statSamplePeriodMs, 
    const std::vector<StatGatheringParams>& statParams);
void runProcessLoopFor(unsigned int sec);

//******************************************************************************
int main(int argc, char **argv) 
{
    char *configFile = NULL;
    int c;
    unsigned int runTimeSec = 0; // default app run time (sec)
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

    if (!configFile || runTimeSec == 0) 
    {
        std::cout << "usage: " << argv[0] << " -c <config_file> -t <app run time in seconds> [-i "
        "<statistics sample interval in milliseconds> -v <verbose mode>]" << std::endl;
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

//******************************************************************************
void run(const std::string &configFile, const ndnlog::NdnLoggerDetailLevel logLevel, 
            const unsigned int runTimeSec, const unsigned int statSamplePeriodMs) 
{
    LibraryObserver libObserver;
    Client& client = Client::getSharedInstance();
    ClientParams params;

    client.setLibraryObserver(libObserver);
    ndnlog::new_api::Logger::getLogger("").setLogLevel(logLevel);

    LogInfo("") << "Run time is set to " << runTimeSec << " seconds, loading "
    "params from " << configFile << "..." << std::endl;

    if (loadParamsFromFile(configFile, params) == EXIT_FAILURE) 
    {
        LogError("") << "error loading params from " << configFile << std::endl;
        return;
    }

    LogInfo("") << "Parameters loaded:\n" << params << std::endl;

    client.run(runTimeSec, statSamplePeriodMs, params);

#warning this is temporary sleep. should fix simple-log for flushing all log records before release
    sleep(1);
    ndnlog::new_api::Logger::releaseAsyncLogging();
    return;
}

// //******************************************************************************
// void removeRemoteStreams(INdnRtcLibrary *ndnp, std::vector<std::string> &StreamsPrefix) {

//     int streamsPrefixNumber = StreamsPrefix.size();

//     for (int streamsPrefixCount = 0; streamsPrefixCount < streamsPrefixNumber; streamsPrefixCount++) {
//         ndnp->removeRemoteStream(StreamsPrefix[streamsPrefixCount]);
//     }
//     return;

// }

