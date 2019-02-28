//
// publish.cpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include "publish.hpp"

#include <chrono>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "../../include/simple-log.hpp"
#include "../../include/helpers/key-chain-manager.hpp"

using namespace std;
using namespace std::chrono;
using namespace ndnrtc;
using namespace ndn;
using namespace ndnlog;
using namespace ndnlog::new_api;

int
readYUV420(FILE *f, int width, int height, uint8_t **buf);

void
runPublisher(string input,
             string basePrefix,
             string streamName,
             string signingIdentity,
             const ndnrtc::VideoStream::Settings& settings,
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

    VideoStream stream(basePrefix, streamName, settings,
                       keyChainManager.instanceKeyChain());

    stream.setLogger(Logger::getLoggerPtr(""));

    int w = settings.codecSettings_.spec_.encoder_.width_;
    int h = settings.codecSettings_.spec_.encoder_.height_;
    size_t imgDataLen = 3*w*h/2;
    uint8_t *imgData = (uint8_t*)malloc(imgDataLen);

    while (readYUV420(fIn, w, h, &imgData))
    {
        vector<boost::shared_ptr<Data>> framePackets = stream.processImage(ImageFormat::I420, imgData);
        // put into memcache
        for (auto d:framePackets){
            // LogDebug("") << "packet " << d->getName() << " : " << d->wireEncode().size() << endl; 
        }

        face->processEvents();
        // TODO setup a timer here for FPS
        // or not if reading from pipe
        usleep(25000);
    }

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
