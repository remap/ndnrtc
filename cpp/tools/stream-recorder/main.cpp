//
// main.cpp
//
//  Created by Peter Gusev on 9 October 2018.
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
#include "../../include/storage-engine.hpp"
#include "stream-recorder.hpp"

static const char USAGE[] =
R"(Stream Recorder.

    Usage:
      stream-recorder <thread_prefix> [--db-path=<db_path> --direction=<dir> | --seed=<seed_frame> | --noverify | --limit=<n_frames> | --pipeline=<p_size> | --lifetime=<ms> | --verbose]

    Arguments:
      <thread_prefix>      ndnrtc (API v3) stream prefix WITH thread name. For example:
                            /ndn/user/rtc/ndnrtc/%FD%03/video/camera/%FC%00%00%01fU%98%BBA/1080p
                           See [ndnrtc namespace](https://github.com/remap/ndnrtc/blob/master/docs/namespace.pdf) for more info.

    Options:
      --db-path=<db_path>  Path for persistent storage DB [default: /tmp/ndnrtc-db]
      --direction=<dir>    Fetching direction: forward, backward, both [default: forward]
      --seed=<seed_frame>  Seed frame to start fetching from. If omitted or zero - starts from the most recent [default: 0]
      --noverify           Specifies, whether verification is not needed
      --limit=<n_frames>   Fetches only n_frames and quits. If omitted or zero - fetches all until stopped [default: 0]
      --lifetime=<ms>      Interests lifetime in milliseconds [default: 3000]
      --pipeline=<p_size>  Specify pipeline size *in frames* [default: 5]
      -v --verbose         Verbose output
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
                         (string("Stream Recorder ")+string(PACKAGE_VERSION)).c_str());  // version string

    // for(auto const& arg : args) {
    //     std::cout << arg.first << " " <<  arg.second << std::endl;
    // }

    ndnlog::new_api::Logger::getLogger("").setLogLevel(args["--verbose"].asBool() ? ndnlog::NdnLoggerDetailLevelAll : ndnlog::NdnLoggerDetailLevelDefault);

    NamespaceInfo prefixInfo;
    if (!NameComponents::extractInfo(args["<thread_prefix>"].asString(), prefixInfo) ||
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

    // setup storage
    boost::shared_ptr<StorageEngine> storage = 
        boost::make_shared<StorageEngine>(args["--db-path"].asString());

    // setup face and keychain
    // TODO: keychain setup for verification
    boost::shared_ptr<Face> face(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<KeyChain> keyChain(boost::make_shared<KeyChain>(boost::make_shared<PibMemory>(), boost::make_shared<TpmBackEndMemory>(),
                                        boost::make_shared<NoVerifyPolicyManager>()));

    // TODO: support other than forward direction for fetching
    // setup settings for stream recorder
    // uint8_t directionMask;
    // StreamRecorder::FetchDirection::Forward
    {
        StreamRecorder recorder(storage, prefixInfo, face, keyChain);
        recorder.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));

        LogInfo("") << "Will fetch stream " << prefixInfo.getPrefix(prefix_filter::Stream) 
            << " (thread " << prefixInfo.threadName_ << ")" << endl;

        StreamRecorder::FetchSettings settings = StreamRecorder::Default;
        settings.pipelineSize_ = args["--pipeline"].asLong();
        settings.lifetime_ = args["--lifetime"].asLong();
        settings.recordLength_ = args["--limit"].asLong();
        settings.seedFrame_ = args["--seed"].asLong();
        if (args["--direction"].asString() == "forward")
            settings.direction_ = StreamRecorder::FetchDirection::Forward;
        if (args["--direction"].asString() == "backward")
            settings.direction_ = StreamRecorder::FetchDirection::Backward;
        if (args["--direction"].asString() == "both")
            settings.direction_ = StreamRecorder::FetchDirection::Forward | StreamRecorder::FetchDirection::Backward;
        
        recorder.start(settings);

        StreamRecorder::Stats stats;

        while (!(err || mustExit))
        {
            if (!args["--verbose"].asBool())
            {
                stats = recorder.getCurrentStats();
                cout << "\r"
                     << "[ " << stats.deltaStored_+stats.keyStored_
                     << " (key " << stats.keyStored_ << " / delta " << stats.deltaStored_ << ")"
                     << " err " << stats.keyFailed_+stats.deltaFailed_ 
                     << " (key " << stats.keyFailed_ << " / delta " << stats.deltaFailed_ << ")"
                     << " mnfst: " << stats.manifestsStored_
                     << " smeta: " << stats.streamMetaStored_
                     << " tmeta: " << stats.threadMetaStored_
                     << " seg: " << stats.totalSegmentsStored_
                     << " key #: " << stats.latestKeyFetched_
                     << " delta #: " << stats.latestDeltaFetched_
                     << " pp: " << stats.pendingFrames_
                     << " ]" << flush;
            }
            usleep(30000);
        }

        recorder.stop();
    }

    LogInfo("") << "Shutting down gracefully..." << endl;

    keyChain.reset();
    // face->shutdown();
    face.reset();
    work.reset();
    // t.join();
    io.stop();

    LogInfo("") << "done" << endl;
}
