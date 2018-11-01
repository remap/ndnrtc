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

static const char USAGE[] =
R"(Networked Storage.

    Usage:
      networked-storage <db_path> [--verbose]

    Arguments:
      <db_path>            Path to persistent storage DB

    Options:
      -v --verbose         Verbose output
)";

using namespace std;
using namespace ndn;
using namespace ndnrtc;

static bool mustExit = false;

void registerPrefix(boost::shared_ptr<Face> &face, const Name &prefix,
                    boost::shared_ptr<StorageEngine> storage);

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

    map<string, docopt::value> args
        = docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,               // show help if requested
                         (string("Netowkred Storage ")+string(PACKAGE_VERSION)).c_str());  // version string

    // for(auto const& arg : args) {
    //     cout << arg.first << " " <<  arg.second << endl;
    // }

    ndnlog::new_api::Logger::getLogger("").setLogLevel(args["--verbose"].asBool() ? ndnlog::NdnLoggerDetailLevelAll : ndnlog::NdnLoggerDetailLevelDefault);

    int err = 0;
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io, &err]() {
        try
        {
            io.run();
        }
        catch (exception &e)
        {
            LogError("") << "Caught exception while running: " << e.what() << endl;
            err = 1;
        }
    });

    // setup storage
    boost::shared_ptr<StorageEngine> storage = 
        boost::make_shared<StorageEngine>(args["<db_path>"].asString(), true);

    // setup face and keychain
    boost::shared_ptr<Face> face = boost::make_shared<ThreadsafeFace>(io);
    boost::shared_ptr<KeyChain> keyChain = boost::make_shared<KeyChain>();

    face->setCommandSigningInfo(*keyChain, keyChain->getDefaultCertificateName());

    LogInfo("") << "Scanning available prefixes..." << std::endl;
    storage->scanForLongestPrefixes(io, [&face, storage](const vector<Name>& pp){
        LogInfo("") << "Scan completed. total keys: " << storage->getKeysNum() 
            << ", payload size ~ " << storage->getPayloadSize()/1024/1024
            << "MB, number of longest prefixes: " << pp.size() << endl;

        for (auto n:pp)
            LogInfo("") << "\t" << n << endl;

        for (auto n:pp)
            registerPrefix(face, n, storage);
    });

    {
        while (!(err || mustExit))
        {
            usleep(10);
        }
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

void registerPrefix(boost::shared_ptr<Face> &face, const Name &prefix, 
    boost::shared_ptr<StorageEngine> storage)
{
    LogInfo("") << "Registering prefix " << prefix << std::endl;
    face->registerPrefix(prefix,
                         [storage](const boost::shared_ptr<const Name> &prefix,
                            const boost::shared_ptr<const Interest> &interest,
                            Face &face, uint64_t, const boost::shared_ptr<const InterestFilter> &) 
                            {
                             LogTrace("") << "Incoming interest " << interest->getName() << std::endl;
                             boost::shared_ptr<Data> d = storage->read(*interest);

                             if (d)
                             {
                                LogTrace("") << "Retrieved data of size " << d->getContent().size() 
                                             << ": " << d->getName() << std::endl;
                                face.putData(*d);
                             }
                             else
                                LogTrace("") << "no data for " << interest->getName() << std::endl;
                         },
                         [](const boost::shared_ptr<const Name> &prefix) 
                         {
                             LogError("") << "Prefix registration failure (" << prefix << ")" << std::endl;
                         },
                         [](const boost::shared_ptr<const Name> &p, uint64_t) 
                         {
                             LogInfo("") << "Successfully registered prefix " << *p << std::endl;
                         });
}