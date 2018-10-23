//
// main.cpp
//
//  Created by Peter Gusev on 9 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include <iostream>
#include <fstream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>

#include "../../contrib/docopt/docopt.h"
#include "../../include/name-components.hpp"
#include "../../include/simple-log.hpp"
#include "../../include/storage-engine.hpp"
#include "../../include/frame-fetcher.hpp"

static const char USAGE[] =
R"(Frame Fetcher.

    Usage:
      frame-fetcher <frame_prefix> [ --verbose | --save=<file_name> ]
      frame-fetcher <frame_prefix> | ffplay -f rawvideo -pixel_format argb -video_size 1280x720 -

    Arguments:
      <frame_prefix>       ndnrtc (API v3) frame prefix (either delta or key).

    Options:
      -s --save=<file_name>  Save output to a file instead of stdout
      -v --verbose           Verbose output
)";

using namespace std;
using namespace ndn;
using namespace ndnrtc;

static bool mustExit = false;

void handler(int sig)
{
    void *array[10];
    size_t size;

    if (sig == SIGABRT || sig == SIGSEGV)
    {
        fprintf(stderr, "Received signal %d:\n", sig);
        // get void*'s for all entries on the stack
        size = backtrace(array, 10);
        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }
    else
        mustExit = true;
}

int main(int argc, char **argv) 
{
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);
    signal(SIGINT, &handler);
    signal(SIGUSR1, &handler);

    ndnlog::new_api::Logger::initAsyncLogging();

    std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,               // show help if requested
                         (string("Frame Fetcher ")+string(PACKAGE_VERSION)).c_str());  // version string

    // for(auto const& arg : args) {
    //     std::cout << arg.first << " " <<  arg.second << std::endl;
    // }

    ndnlog::new_api::Logger::getLogger("").setLogLevel(args["--verbose"].asBool() ? ndnlog::NdnLoggerDetailLevelAll : ndnlog::NdnLoggerDetailLevelDefault);

    NamespaceInfo prefixInfo;
    if (!NameComponents::extractInfo(args["<frame_prefix>"].asString(), prefixInfo) ||
            prefixInfo.threadName_ == "")
    {
        LogError("") << "Bad thread prefix provided" << endl;
        exit(1);
    }

    int err = 0;
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io, &err]() {
        try
        {
            io.run();
        }
        catch (std::exception &e)
        {
            LogError("") << "Client caught exception while running: " << e.what() << std::endl;
            err = 1;
        }
    });

    boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<KeyChain> keyChain = boost::make_shared<KeyChain>();

    {
        static uint8_t *frameBuffer = nullptr;
        FrameFetcher ffetcher(face, keyChain);

        if (args["--verbose"].asBool())
            ffetcher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));

        ffetcher.fetch(prefixInfo.getPrefix(prefix_filter::Sample),
                [](const boost::shared_ptr<IFrameFetcher>&, int width, int height)
                {
                    LogTrace("") << "allocating buffer for " 
                                 << width << "x" << height << " frame" << std::endl;
                    // allocating ARGB buffer
                    frameBuffer = (uint8_t*)malloc(width*height*4);
                    return frameBuffer;
                },
                [&args](const boost::shared_ptr<IFrameFetcher>&, const FrameInfo&, int nFramesFetched,
                   int width, int height, const uint8_t* buffer)
                {
                    LogTrace("") << "received frame" << std::endl;
                    mustExit = true;

                    if (args["--save"].isString())
                    {
                        stringstream fileName;
                        fileName << args["--save"].asString() << "." << width << "x" << height;

                        ofstream frameFile;
                        frameFile.open(fileName.str(), ios::binary);
                        frameFile.write((const char*)buffer, width*height*4);
                        frameFile.close();
                    }
                    else
                        fwrite(buffer, 1, width*height*4, stdout);
                },
                [&err](const boost::shared_ptr<IFrameFetcher>&, std::string reason)
                {
                    LogError("") << "failed to retrieve frame: " << reason << std::endl;
                    err = 1;
                });

        while (!(err || mustExit))
        {
            usleep(30000);
        }

    }

    keyChain.reset();
    face->shutdown();
    face.reset();
    work.reset();
    t.join();
    io.stop();
}
