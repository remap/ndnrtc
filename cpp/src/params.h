//
//  NdnParams.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/23/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef ndnrtc_NdnParams_h
#define ndnrtc_NdnParams_h

#include "ndnrtc-common.h"

#define PARAM_OK(p, l, r) (p >= l && p <= r)
#define PARAM_POS(p) (p > 0)
#define PARAM_GE(p, l) (p >= l)
#define PARAM_LE(p, r) (p <= r)
#define PARAM_VALIDATE(param, lb, rb, res) (ParamsStruct::validateParam(param, lb, rb, res, DefaultParams.##param##))
#define PARAM_VALIDATE_LE(param, rb, res) (PARAM_VALIDATE(param, 0, rb, res))
#define PARAM_VALIDATE_GE(param, lb, res) (PARAM_VALIDATE(param, lb, (unsigned int)-1, res))


namespace ndnrtc
{
    static const unsigned int
        MaxWidth = 5000,
        MaxHeight = 5000,
        MaxPortNum = 65535,
        MaxStartBitrate = 50000,
        MaxBitrate = 500000,
        MaxFrameRate = 120;
    

    typedef struct _ParamsStruct {
        NdnLoggerDetailLevel loggingLevel;
        const char *logFile;
        
        // capture settings
        unsigned int captureDeviceId;
        unsigned int captureWidth, captureHeight;
        unsigned int captureFramerate;
        
        // render
        unsigned int renderWidth, renderHeight;
        
        // codec
        unsigned int codecFrameRate;
        unsigned int startBitrate, maxBitrate;
        unsigned int encodeWidth, encodeHeight;
        
        // network parameters
        const char *host;
        unsigned int portNum;
        
        // ndn producer
        const char *producerId;
        const char *streamName;
        const char *streamThread;
        const char *ndnHub;
        unsigned int segmentSize, freshness, producerRate;
        
        // ndn consumer
        unsigned int playbackRate;
        unsigned int interestTimeout;
        unsigned int bufferSize, slotSize;
        
        static unsigned int validate(unsigned int param,
                                     unsigned int lb,
                                     unsigned int rb,
                                     int &res,
                                     unsigned int defParam)
        {
            int result = (PARAM_OK(param, lb, rb))?RESULT_OK:RESULT_WARN;
            
            if (RESULT_NOT_OK(result))
                res = result;
            
            return (result == RESULT_OK)?param:defParam;
        }
        static unsigned int validateLE(unsigned int param, unsigned int rb,
                                       int &res, unsigned int defParam)
        {
            return validate(param, 0, rb, res, defParam);
        }
        static unsigned int validateGE(unsigned int param, unsigned int lb,
                                       int &res, unsigned int defParam)
        {
            return validate(param, lb, (unsigned int)-1, res, defParam);
        }
        static std::string validate(const char *param, const char *defParam,
                                    int &res)
        {
            int result = (param != NULL)?RESULT_OK:RESULT_WARN;
            
            if (RESULT_NOT_OK(result))
                res = result;
            
            return string((RESULT_GOOD(result)?param:defParam));
        }

    } ParamsStruct;
    
    static ParamsStruct DefaultParams = {
        NdnLoggerDetailLevelDefault,    // log level
        "ndnrtc.log",                   // log file
        
        0,      // capture device id
        640,    // capture width
        480,    // capture height
        30,     // capture framerate
        
        640,    // render width
        480,    // render height
        
        30,     // codec framerate
        300,    // codec start bitrate
        4000,   // codec max bitrate
        640,    // codec encoding width
        480,    // codec encoding height
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "video0",       // stream name
        "vp8",          // stream thread name
        "ndn/ucla.edu/apps",     // ndn hub
        1100,   // segment size for media frame
        3,      // data freshness (seconds) value
        30,     // producer rate (currently equal to playback rate)
        
        30,     // playback rate of local consumer
        5,      // interest timeout
        60,     // incoming framebuffer size
        16000  // frame buffer slot size
    };
    
    static ParamsStruct DefaultParamsAudio = {
        NdnLoggerDetailLevelDefault,    // log level
        "ndnrtc.log",                   // log file
        
        0,      // capture device id
        0,    // capture width
        0,    // capture height
        0,     // capture framerate
        
        0,    // render width
        0,    // render height
        
        0,     // codec framerate
        0,    // codec start bitrate
        0,   // codec max bitrate
        0,    // codec encoding width
        0,    // codec encoding height
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "audio0",       // stream name
        "pcmu2",          // stream thread name
        "ndn/ucla.edu/apps",     // ndn hub
        1100,   // segment size for media frame
        3,      // data freshness (seconds) value
        30,     // producer rate (currently equal to playback rate)
        
        30,     // playback rate of local consumer
        5,      // interest timeout
        10,     // incoming framebuffer size
        1000  // frame buffer slot size
    };

}

#endif
