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
#include "../src/clock.hpp"

#include "precise-generator.hpp"
#include "stat.hpp"

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

void
runPublishing(boost::asio::io_service &io,
              string input,
              string basePrefix,
              string streamName,
              string signingIdentity,
              const VideoStream::Settings& settings,
              bool needRvp,
              int nLoops,
              string csv,
              string stats)
{
    FILE *fIn = fopen(input.c_str(), "rb");
    if (!fIn)
        throw runtime_error("couldn't open input file "+input);

    stringstream instanceId;
    instanceId << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    boost::shared_ptr<Face> face = boost::make_shared<ThreadsafeFace>(io);
    helpers::KeyChainManager keyChainManager(face, boost::make_shared<KeyChain>(),
        signingIdentity, instanceId.str(), "", 3600,
        Logger::getLoggerPtr(AppLog));

    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(face.get());
    memCache->setMinimumCacheLifetime(5000); // keep last 5 seconds of data
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
    bool isLooped = (nLoops == 0 ? true : false);

    do
    {
        auto publish = [&](){
            res = readYUV420(fIn, w, h, &imgData);

            if (res == 0)
            {
                if(feof(fIn))
                {
                    if (isLooped || --nLoops > 0)
                    {
                        rewind(fIn);
                        res = readYUV420(fIn, w, h, &imgData);
                    }
                    else
                    {
                        io.stop();
                        return;
                    }
                }
                else
                {
                    LogError(AppLog) << "error reading frame "
                                     << errno << ": " << strerror(errno) << endl;
                    io.stop();
                    return; // error
                }
            }

            vector<boost::shared_ptr<Data>> framePackets = stream.processImage(ImageFormat::I420, imgData);
            size_t bytesRaw = 0;
            for (auto &d:framePackets)
                bytesRaw += d->getDefaultWireEncoding().size();
            
//            cout << stream.getSeqNo()
//                 << "\t" << stream.getGopNo()
//                 << "\t" << framePackets.size()
//                 << "\t" << bytesRaw
//                 << endl;
//            if (stream.getSeqNo() == 1000)
//                exit(0);
            if (AppLog != "" ||
                (AppLog == "" && Logger::getLoggerPtr(AppLog)->getLogLevel() <= ndnlog::NdnLoggerDetailLevelDefault))
                printStats(stream, framePackets);
            
            dumpStats(csv, stats, stream.getStatistics());
            
        };

        PreciseGenerator gen(io,
                            settings.codecSettings_.spec_.encoder_.fps_,
                            publish);
        gen.start();
        io.run();
        gen.stop();
    } while (res && !MustTerminate);

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
                             LogError(AppLog) << "Prefix registration failure (" << *prefix << ")" << std::endl;
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
    static uint64_t startTime = 0;
    StatisticsStorage ss(s.getStatistics());

    if (startTime == 0)
        startTime = clock::millisecondTimestamp();
    
    cout << "\r"
         << "[ "
         << setw(5) << setprecision(2) << (clock::millisecondTimestamp() - startTime)/1000. << "sec "
         << s.getPrefix()
         << ": pub rate " << ss[Indicator::CurrentProducerFramerate]
         << " processed " << (uint32_t)ss[Indicator::ProcessedNum]
         << "/" << (uint32_t)ss[Indicator::DroppedNum]
         << " "
         << (uint32_t)ss[Indicator::PublishedKeyNum] << "k"
         << " enc " << setw(4) << setprecision(3) << ss[Indicator::CodecDelay] << "ms"
         << " pub " << ss[Indicator::PublishDelay] << "ms"
         << fixed
         << " payload " << (uint64_t)ss[Indicator::BytesPublished]
         // TODO: why raw is much smaller than payload? check getDefaultWireEncoding()
         << " b raw " << (uint64_t)ss[Indicator::RawBytesPublished]
         << " b sign " << (uint32_t)ss[Indicator::SignNum]
         << " ]" << flush;
}
