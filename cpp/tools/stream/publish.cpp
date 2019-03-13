//
// publish.cpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include "publish.hpp"

#include <chrono>
#include <mutex>
#include <boost/chrono.hpp>
#include <execinfo.h>

#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "../../include/simple-log.hpp"
#include "../../include/helpers/key-chain-manager.hpp"
#include "../../include/statistics.hpp"

#include "precise-generator.hpp"

static bool mustTerminate = false;

using namespace std;
using namespace std::chrono;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndnrtc::helpers;
using namespace ndn;
using namespace ndnlog;
using namespace ndnlog::new_api;

int readYUV420(FILE *f, int width, int height, uint8_t **buf);
void registerPrefix(boost::shared_ptr<Face> &face, const KeyChainManager &keyChainManager);
void serveCerts(boost::shared_ptr<Face> &face, KeyChainManager &keyManager);
void printStats(const VideoStream&, const vector<boost::shared_ptr<Data>>&);



void terminate(boost::asio::io_service &ioService,
               const boost::system::error_code &error,
               int signalNo,
               boost::asio::signal_set &signalSet)
{
    if (error)
        return;

    std::cerr << "caught signal '" << strsignal(signalNo) << "', exiting..." << std::endl;
    ioService.stop();

    if (signalNo == SIGABRT || signalNo == SIGSEGV)
    {
        void *array[10];
        size_t size;
        size = backtrace(array, 10);
        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
    }

    mustTerminate = true;
}

void
runPublisher(string input,
             string basePrefix,
             string streamName,
             string signingIdentity,
             const VideoStream::Settings& settings,
             bool needRvp,
             bool isLooped)
{
    FILE *fIn = fopen(input.c_str(), "rb");
    if (!fIn)
        throw runtime_error("couldn't open input file "+input);

    stringstream instanceId;
    instanceId << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    boost::asio::io_context io;

    boost::asio::signal_set signalSet(io);
    signalSet.add(SIGINT);
    signalSet.add(SIGTERM);
    signalSet.add(SIGABRT);
    signalSet.add(SIGSEGV);
    signalSet.add(SIGHUP);
    signalSet.add(SIGUSR1);
    signalSet.add(SIGUSR2);
    signalSet.async_wait(bind(static_cast<void(*)(boost::asio::io_service&,const boost::system::error_code&,int,boost::asio::signal_set&)>(&terminate),
                              ref(io),
                              _1, _2,
                              ref(signalSet)));

    boost::shared_ptr<Face> face = boost::make_shared<ThreadsafeFace>(io);
    helpers::KeyChainManager keyChainManager(face, boost::make_shared<KeyChain>(),
        signingIdentity, instanceId.str(), "", 3600,
        Logger::getLoggerPtr(AppLog));

    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(face.get());
    memCache->setMinimumCacheLifetime(5000); // keep last 5 seconds of datandnr
    VideoStream::Settings s(settings);
    s.memCache_ = memCache;
    s.storeInMemCache_ = true;

    // setup for serving certs
    registerPrefix(face, keyChainManager);
    serveCerts(face, keyChainManager);

    // setup mem cache for serving data
    // register data prefix if it's different from instance identity
    memCache->registerPrefix(basePrefix,
                             [](const boost::shared_ptr<const Name> &prefix) {
                                 LogError(AppLog) << "Prefix registration failure (" << prefix << ")" << std::endl;
                                 throw std::runtime_error("Prefix registration failed");
                             });

    VideoStream stream(basePrefix, streamName, s,
                       keyChainManager.instanceKeyChain());
    stream.setLogger(Logger::getLoggerPtr(AppLog));

    int w = settings.codecSettings_.spec_.encoder_.width_;
    int h = settings.codecSettings_.spec_.encoder_.height_;
    uint32_t sampleIntervalUsec = 1000000 / settings.codecSettings_.spec_.encoder_.fps_;
    size_t imgDataLen = 3*w*h/2;
    uint8_t *imgData = (uint8_t*)malloc(imgDataLen);

    LogDebug(AppLog) << "publishing under " << stream.getPrefix()
                 << " sample interval " << sampleIntervalUsec << "us" << endl;

    int res = 0;

    do
    {
        function<void()> publish = [&](){
            res = readYUV420(fIn, w, h, &imgData);
            if (res == 0)
            {
                if(feof(fIn))
                {
                    if (isLooped)
                    {
                        rewind(fIn);
                        res = readYUV420(fIn, w, h, &imgData);
                    }
                }
                else
                    return; // error
            }

            vector<boost::shared_ptr<Data>> framePackets = stream.processImage(ImageFormat::I420, imgData);
            // for (auto d:framePackets)
            //     LogDebug(AppLog) << "packet " << d->getName() << " : " << d->wireEncode().size() << " bytes" << endl;
            if (AppLog != "")
                printStats(stream, framePackets);
        };

        PreciseGenerator gen(io,
                            settings.codecSettings_.spec_.encoder_.fps_,
                            publish);
        gen.start();
        io.run();
        gen.stop();
    } while (res && !mustTerminate);

    // end line after stats
    cout << endl;

    fclose(fIn);
}

int
readYUV420(FILE *f, int width, int height, uint8_t **imgData)
{
    size_t len = 3*width*height/2;
    uint8_t *buf = *imgData;

    for (int plane = 0; plane < 3; ++plane) {
        const int stride = plane == 0 ? width : width/2;
        const int w = plane == 0 ? width : (width+1) >> 1;
        const int h = plane == 0 ? height : (height+1) >> 1;

        for (int y = 0; y < h; ++y)
        {
            if (fread(buf, 1, w, f) != (size_t)w) return 0;
            buf += stride;
        }
    }

    return 1;
}

void
registerPrefix(boost::shared_ptr<Face> &face,
               const KeyChainManager &keyChainManager)
{
    face->registerPrefix(Name(keyChainManager.instancePrefix()),
                         [](const boost::shared_ptr<const Name> &prefix,
                            const boost::shared_ptr<const Interest> &interest,
                            Face &face, uint64_t, const boost::shared_ptr<const InterestFilter> &) {
                             LogTrace(AppLog) << "Unexpected incoming interest " << interest->getName() << std::endl;
                         },
                         [](const boost::shared_ptr<const Name> &prefix) {
                             LogError(AppLog) << "Prefix registration failure (" << prefix << ")" << std::endl;
                             throw std::runtime_error("Prefix registration failed");
                         },
                         [](const boost::shared_ptr<const Name> &p, uint64_t) {
                             LogDebug(AppLog) << "Successfully registered prefix " << *p << std::endl;
                         });
}

void
serveCerts(boost::shared_ptr<Face> &face, KeyChainManager &keyManager)
{
    static MemoryContentCache cache(face.get());
    Name filter;

    if (keyManager.defaultKeyChain()->getIsSecurityV1())
        filter = keyManager.instanceCertificate()->getName().getPrefix(keyManager.instanceCertificate()->getName().size() - 1);
    else
        filter = keyManager.instanceCertificate()->getName().getPrefix(keyManager.instanceCertificate()->getName().size() - 2);

    LogDebug(AppLog) << "setting interest filter for instance certificate: " << filter << std::endl;

    cache.setInterestFilter(filter);
    cache.add(*(keyManager.instanceCertificate().get()));
}

void printStats(const VideoStream& s, const vector<boost::shared_ptr<Data>>& packets)
{
    StatisticsStorage ss(s.getStatistics());

    cout << "\r"
         << "[ " << s.getPrefix()
        << ": pub rate " << ss[Indicator::CurrentProducerFramerate]
         << " processed " << (uint32_t)ss[Indicator::ProcessedNum]
         << "/" << (uint32_t)ss[Indicator::DroppedNum]
         << " "
         << (uint32_t)ss[Indicator::PublishedKeyNum] << "k"
         << fixed
         << " payload " << (uint64_t)ss[Indicator::BytesPublished]
         // TODO: why raw is much smaller than payload? check getDefaultWireEncoding()
         << " b raw " << (uint64_t)ss[Indicator::RawBytesPublished]
         << " b sign " << (uint32_t)ss[Indicator::SignNum]
         << " ]" << flush;
}
