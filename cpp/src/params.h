//
//  NdnParams.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef ndnrtc_NdnParams_h
#define ndnrtc_NdnParams_h

#include "ndnrtc-common.h"

#define PARAM_OK(p, l, r) (p >= l && p <= r)

namespace ndnrtc
{
    static const unsigned int
        MaxWidth = 5000,
        MinWidth = 100,
        MaxHeight = 5000,
        MinHeight = 100,
        MaxPortNum = 65535,
        MaxStartBitrate = 50000,
        MinStartBitrate = 100,
        MaxBitrate = 500000,
        MaxFrameRate = 60,
        MinFrameRate = 1,
        MaxSegmentSize = 20000,
        MinSegmentSize = 10,
        MaxSlotSize = 20000,
        MinSlotSize = 10,
        MaxBufferSize = 5*120,
        MinBufferSize = 1;
    

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
        unsigned int codecFrameRate, gop;
        unsigned int startBitrate, maxBitrate;
        unsigned int encodeWidth, encodeHeight;
        bool dropFramesOn;
        
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
        unsigned int jitterSize DEPRECATED;
        
        /**
         * Validates video parameters
         * If any of the values is not valid, uses default value instead for 
         * that parameter and returns with RESULT_WARN code. validated variable
         * contains validated parameters in this case.
         * If any of the critical values are not valid (i.e. bad produced id)
         * returns RESULT_ERR. validated variable is undefined in this case.
         * @param params Parameters to be validated
         * @param validated Validated parameters
         * @return  RESULT_ERR  If could not validate parameters
         *          RESULT_WARN If could validate parameters, though some values 
         *                      were not valid and default values were used
         *                      instead
         *          RESULT_OK   If parameters are valid
         */
        static int validateVideoParams(const struct _ParamsStruct &params,
                                       struct _ParamsStruct &validated);
        static int validateAudioParams(const struct _ParamsStruct &params,
                                       struct _ParamsStruct &validated);
        
        static unsigned int validate(unsigned int param,
                                     unsigned int lb,
                                     unsigned int rb,
                                     int &res,
                                     unsigned int defParam) //DEPRECATED
        {
            int result = (PARAM_OK(param, lb, rb))?RESULT_OK:RESULT_WARN;
            
            if (RESULT_NOT_OK(result))
                res = result;

            return (result == RESULT_OK)?param:defParam;
        }
        static unsigned int validateLE(unsigned int param, unsigned int rb,
                                       int &res, unsigned int defParam) //DEPRECATED
        {
            return validate(param, 0, rb, res, defParam);
        }
        static unsigned int validateGE(unsigned int param, unsigned int lb,
                                       int &res, unsigned int defParam) //DEPRECATED
        {
            return validate(param, lb, (unsigned int)-1, res, defParam);
        }
        static std::string validate(const char *param, const char *defParam,
                                    int &res) //DEPRECATED
        {
            int result = (param != NULL)?RESULT_OK:RESULT_WARN;
            
            if (RESULT_NOT_OK(result))
                res = result;
            
            return string((RESULT_GOOD(result)?param:defParam));
        }
    } ParamsStruct;
    
    static ParamsStruct DefaultParams = {
        NdnLoggerDetailLevelDebug,    // log level
        "ndnrtc.log",                   // log file
        
        0,      // capture device id
        640,    // capture width
        480,    // capture height
        30,     // capture framerate
        
        640,    // render width
        480,    // render height
        
        30,     // codec framerate
        60,     // gop
        2000,    // codec start bitrate
        10000,   // codec max bitrate
        640,    // codec encoding width
        480,    // codec encoding height
        1,      // instruct encoder to drop frames if cannot keep up with the
                // maximum bitrate
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "video0",       // stream name
        "vp8",          // stream thread name
        "ndn/ucla.edu/apps",     // ndn hub
        1100,   // segment size for media frame
        5,      // data freshness (seconds) value
        30,     // producer rate (currently equal to playback rate)
        
        30,     // playback rate of local consumer
        5,      // interest timeout
        30,     // incoming framebuffer size
        16000,  // frame buffer slot size
        20       // jitter size
    };
    
    // only some parameters are used for audio configuration (those that are
    // not 0's/empty strings)
    static ParamsStruct DefaultParamsAudio = {
        NdnLoggerDetailLevelNone,    // log level
        "",                   // log file
        
        0,      // capture device id
        0,    // capture width
        0,    // capture height
        0,     // capture framerate
        
        0,    // render width
        0,    // render height
        
        0,     // codec framerate
        INT_MAX,      // gop
        0,    // codec start bitrate
        0,   // codec max bitrate
        0,    // codec encoding width
        0,    // codec encoding height
        0,      // drop frames
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "audio0",       // stream name
        "pcmu2",          // stream thread name
        "ndn/ucla.edu/apps",     // ndn hub
        1000,   // segment size for media frame
        5,      // data freshness (seconds) value
        50,     // producer rate (currently equal to playback rate)
        
        50,     // playback rate of local consumer
        2,      // interest timeout
        20,     // incoming framebuffer size
        1000,  // frame buffer slot size
        33      // jitter size
    };

}

#endif
