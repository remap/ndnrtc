// 
// local-stream.cc
//
//  Created by Peter Gusev on 18 July 2016.
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
#include "../client/src/config.h"

using namespace std;
using namespace ndnrtc;

void run(const std::string &configFile,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSecconst,
         const unsigned int statisticsSampleInterval);

int main(int argc, char **argv)
{
#if 0
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
#endif
	return 0;
}

void run(const std::string &configFile, const ndnlog::NdnLoggerDetailLevel logLevel, 
            const unsigned int runTimeSec, const unsigned int statSamplePeriodMs) 
{
#if 0
    ClientParams params;

	ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(logLevel);

    LogInfo("") << "Run time is set to " << runTimeSec << " seconds, loading "
    "params from " << configFile << "..." << std::endl;

    if (loadParamsFromFile(configFile, params) == EXIT_FAILURE) 
    {
        LogError("") << "error loading params from " << configFile << std::endl;
        return;
    }

    LogInfo("") << "Parameters loaded:\n" << params << std::endl;

    //client.run(&ndnp, runTimeSec, statSamplePeriodMs, params);

#warning this is temporary sleep. should fix simple-log for flushing all log records before release
    sleep(1);
    ndnlog::new_api::Logger::releaseAsyncLogging();
    return;
#endif
}
