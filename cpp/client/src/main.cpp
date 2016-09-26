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

#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>

#include "config.hpp"
#include "client.hpp"
#include "key-chain-manager.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

void run(const std::string &configFile,
         const std::string &identity,
         const ndnlog::NdnLoggerDetailLevel appLoggingLevel,
         const unsigned int headlessAppOnlineTimeSecconst,
         const unsigned int statisticsSampleInterval);

//******************************************************************************
int main(int argc, char **argv) 
{
    char *configFile = NULL, *identity = NULL, *policy = NULL;
    int c;
    unsigned int runTimeSec = 0; // default app run time (sec)
    unsigned int statSamplePeriodMs = 100;  // default statistics sample interval (ms)
    ndnlog::NdnLoggerDetailLevel logLevel = ndnlog::NdnLoggerDetailLevelDefault;

    opterr = 0;
    while ((c = getopt (argc, argv, "vi:t:c:s:p:")) != -1)
        switch (c) {
        case 'c':
            configFile = optarg;
            break;
        case 's':
            identity = optarg;
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

    if (!configFile || runTimeSec == 0 || identity == NULL) 
    {
        std::cout << "usage: " << argv[0] << " -c <config_file> -s <signing identity>"
            "-t <app run time in seconds> [-i <statistics sample interval in milliseconds>"
            " -v <verbose mode>]" << std::endl;
        exit(1);
    }

    run(configFile, identity, logLevel, runTimeSec, statSamplePeriodMs);

    return 0;
}

//******************************************************************************
void run(const std::string &configFile, const std::string &identity, 
    const ndnlog::NdnLoggerDetailLevel logLevel, const unsigned int runTimeSec, 
    const unsigned int statSamplePeriodMs) 
{
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(logLevel);

    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){ io.run(); });
    KeyChainManager keyChainManager(identity, runTimeSec);
    boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
    ClientParams params;

    LogInfo("") << "Run time is set to " << runTimeSec << " seconds, loading "
    "params from " << configFile << "..." << std::endl;

    if (loadParamsFromFile(configFile, params, identity) == EXIT_FAILURE) 
    {
        LogError("") << "error loading params from " << configFile << std::endl;
        return;
    }

    LogInfo("") << "Parameters loaded" << std::endl;
    LogDebug("") << params << std::endl;

    Client client(io, face, keyChainManager.instanceKeyChain());
    client.run(runTimeSec, statSamplePeriodMs, params);

    face->shutdown();
    work.reset();
    t.join();
    io.stop();

    LogInfo("") << "Client run completed" << std::endl;

#warning this is temporary sleep. should fix simple-log for flushing all log records before release
    sleep(1);
    ndnlog::new_api::Logger::releaseAsyncLogging();
    return;
}
