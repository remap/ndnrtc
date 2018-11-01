//
// main.cpp
//
//  Created by Peter Gusev on 29 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include <iostream>
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
#include "../../include/remote-stream.hpp"
#include "renderer.hpp"

static const char USAGE[] =
R"(Stream Scrubber.

    Usage:
      stream-scrubber <thread_prefix> ( <out_pipe> | - ) [--seed_frame=<frame_no>] [--loop=<n_frames>] [--speed=<speed>] [--direction=<dir>] [--noverify] [--pipeline=<p_size>] [--buffer=<ms>] [--verbose]

    Arguments:
      <thread_prefix>      ndnrtc (API v3) thread prefix with or without frame number. For example:
                            /ndn/user/rtc/ndnrtc/%FD%03/video/camera/%FC%00%00%01fU%98%BBA/1080p/k/%FE%2A
                           If frame number is omitted, it must be specified usin --frame-no argument. It is assumed, that
                           the key frame number is provided.
                           See [ndnrtc namespace](https://github.com/remap/ndnrtc/blob/master/docs/namespace.pdf) for more info.
      <out_pipe>           Output pipe where received decoded ARGB frames will be dumped

    Options:
      --seed_frame=<frame_no>   Key frame number to start scrubbing from
      --loop=<n_frames>         Will loop playing back <n_frames> starting from <frame_N> [default: 0]. Note, application will continue requesting frames from the network and won't store it in-memory
      --speed=<speed>           Speed of scrubbing, specified as a float multiplier to the streams' default framerate [default: 1]
      --direction=<dir>         Scrubbing direction: forward, backward
      --noverify                Specifies, whether verification is not needed
      --buffer=<ms>             Buffer size in milliseconds [default: 150]
      --pipeline=<p_size>       Specify pipeline size *in frames* [default: 5]
      -v --verbose              Verbose output
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
    signal(SIGINT, handler);
    signal(SIGUSR1, handler);

    ndnlog::new_api::Logger::initAsyncLogging();

    std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,               // show help if requested
                         (string("Stream Scrubber ")+string(PACKAGE_VERSION)).c_str());  // version string

    // for(auto const& arg : args) {
    //     std::cout << arg.first << " " <<  arg.second << std::endl;
    // }

    ndnlog::new_api::Logger::getLogger("").setLogLevel(args["--verbose"].asBool() ? ndnlog::NdnLoggerDetailLevelDebug : ndnlog::NdnLoggerDetailLevelDefault);

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

    // setup face and keychain
    boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
    // TODO: setup verify/noverify properly
    boost::shared_ptr<KeyChain> keyChain(boost::make_shared<KeyChain>(boost::make_shared<PibMemory>(), 
                                         boost::make_shared<TpmBackEndMemory>(),
                                         boost::make_shared<NoVerifyPolicyManager>()));



    // process thread prefix
    NamespaceInfo prefixInfo;

    if (!NameComponents::extractInfo(args["<thread_prefix>"].asString(), prefixInfo) ||
        prefixInfo.threadName_ == "")
    {
        LogError("") << "Bad thread prefix provided" << endl;
        exit(1);
    }

    int bufferSize = args["--buffer"].asLong();

    RemoteVideoStream::FetchingRuleSet ruleset;
    ruleset.seedKeyNo_ = (args["--seed_frame"].isLong() || args["--seed_frame"].isString()) ? args["--seed_frame"].asLong() : (prefixInfo.class_ == SampleClass::Key ? prefixInfo.sampleNo_ : 0);
    ruleset.skipDelta_ = false;
    ruleset.loop_ = args["--loop"].asLong();
    ruleset.speed_ = (float)(args["--speed"].asLong());
    ruleset.pipelineSize_ = args["--pipeline"].asLong();

    LogInfo("") << "will fetch from " 
                << prefixInfo.getPrefix(prefix_filter::Thread)
                << " seed key frame #" << ruleset.seedKeyNo_
                << " pipeline size " << ruleset.pipelineSize_ 
                << " buffer size " << bufferSize << endl;

    {
        RemoteVideoStream stream(io, face, keyChain, 
                                 prefixInfo.getPrefix(prefix_filter::Thread).toUri(), 
                                 bufferSize);
        stream.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
        
        Renderer renderer(args["<out_pipe>"].isString() ? args["<out_pipe>"].asString() : "-");

        stream.start(ruleset, &renderer);

        while (!(err || mustExit))
        {
            usleep(30000);
        }

        stream.stop();
    }

    LogInfo("") << "Shutting down gracefully..." << endl;

    keyChain.reset();
    face->shutdown();
    face.reset();
    work.reset();
    t.join();
    io.stop();

    LogInfo("") << "done" << endl;
}
