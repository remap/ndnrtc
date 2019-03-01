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

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "../../include/simple-log.hpp"
#include "../../include/helpers/key-chain-manager.hpp"
#include "../../include/statistics.hpp"

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
runPublisher(string input,
             string basePrefix,
             string streamName,
             string signingIdentity,
             const VideoStream::Settings& settings,
             bool needRvp)
{
    FILE *fIn = fopen(input.c_str(), "rb");
    if (!fIn)
        throw runtime_error("couldn't open input file "+input);

    stringstream instanceId;
    instanceId << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    boost::shared_ptr<Face> face = boost::make_shared<Face>();
    helpers::KeyChainManager keyChainManager(face, boost::make_shared<KeyChain>(),
        signingIdentity, instanceId.str(), "", 3600,
        Logger::getLoggerPtr(""));

    // NOTE: 0 cleanup inerval is important for high-frequency data like frames
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(face.get(),0);
    VideoStream::Settings s(settings);
    s.memCache_ = memCache;
    s.storeInMemCache_ = true;

    registerPrefix(face, keyChainManager);
    serveCerts(face, keyChainManager);

    VideoStream stream(basePrefix, streamName, s,
                       keyChainManager.instanceKeyChain());

    stream.setLogger(Logger::getLoggerPtr(""));

    int w = settings.codecSettings_.spec_.encoder_.width_;
    int h = settings.codecSettings_.spec_.encoder_.height_;
    uint32_t sampleIntervalUsec = 1000000 / settings.codecSettings_.spec_.encoder_.fps_;
    size_t imgDataLen = 3*w*h/2;
    uint8_t *imgData = (uint8_t*)malloc(imgDataLen);
    boost::asio::io_context io;

    LogDebug("") << "publishing under " << stream.getPrefix() << endl;

    while (readYUV420(fIn, w, h, &imgData))
    {
        boost::asio::steady_timer timer(io, microseconds(sampleIntervalUsec));
        vector<boost::shared_ptr<Data>> framePackets = stream.processImage(ImageFormat::I420, imgData);
        for (auto d:framePackets)
            LogDebug("") << "packet " << d->getName() << " : " << d->wireEncode().size() << " bytes" << endl;

        face->processEvents();
        printStats(stream, framePackets);
        timer.wait();
    }

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
                             LogTrace("") << "Unexpected incoming interest " << interest->getName() << std::endl;
                         },
                         [](const boost::shared_ptr<const Name> &prefix) {
                             LogError("") << "Prefix registration failure (" << prefix << ")" << std::endl;
                             throw std::runtime_error("Prefix registration failed");
                         },
                         [](const boost::shared_ptr<const Name> &p, uint64_t) {
                             LogDebug("") << "Successfully registered prefix " << *p << std::endl;
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

    LogDebug("") << "setting interest filter for instance certificate: " << filter << std::endl;

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
