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
#include "config.h"


using namespace std;
using namespace ndnrtc;

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

	return 0;
}
