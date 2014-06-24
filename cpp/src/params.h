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

#include <cstdlib>
#include <sstream>

#include "simple-log.h"
#include "ndnrtc-defines.h"

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
        MinBufferSize = 1,
        MinJitterSize = 150;
    

    typedef struct _ParamsStruct {
        ndnlog::NdnLoggerDetailLevel loggingLevel;
        const char *logFile;
        bool useTlv, useRtx, useFec, useCache, useAudio, useVideo, useAvSync;
        unsigned int headlessMode;
        
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
        unsigned int segmentSize, freshness;
        double producerRate;
        bool skipIncomplete;
        
        // ndn consumer
        unsigned int playbackRate;
        unsigned int interestTimeout;
        unsigned int bufferSize, slotSize;
        unsigned int jitterSize;
        
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
            
            return std::string((RESULT_GOOD(result)?param:defParam));
        }
        
        void setProducerId(const char* producerId)
        {   
            this->producerId = (char*)malloc(strlen(producerId)+1);
            memset((void*)this->producerId, 0, strlen(producerId));
            strcpy((char*)this->producerId, producerId);
        }
    } ParamsStruct;
    
    static ParamsStruct DefaultParams = {
        ndnlog::NdnLoggerDetailLevelDebug,    // log level
        "ndnrtc.log\0",                   // log file
        true,   // use TLV encoding
        true,   // reissue timed out interests
        true,   // use FEC
        true,   // use memory cache
        true,   // use av sync
        true,   // use audio
        true,   // use video
        0,      // headless mode off
        
        0,      // capture device id
        640,    // capture width
        480,    // capture height
        30,     // capture framerate
        
        640,    // render width
        480,    // render height
        
        30,     // codec framerate
        30,     // gop
        1000,   // codec start bitrate
        10000,  // codec max bitrate
        640,    // codec encoding width
        480,    // codec encoding height
        1,      // instruct encoder to drop frames if cannot keep up with the
                // maximum bitrate
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "video0",       // stream name
        "vp8",          // stream thread name
        "ndn/edu/ucla/apps",     // ndn hub
        1054,   // segment size for media frame (MTU - NDN header (currently 446 bytes))
        5,      // data freshness (seconds) value
        30,     // producer rate (currently equal to playback rate)
        true,   // skip incomplete frames
        
        30,     // playback rate of local consumer
        5000,   // interest timeout
        90,     // assembling buffer size
        16000,  // frame buffer slot size
        150     // jitter buffer size in ms
    };
    
    // only some parameters are used for audio configuration (those that are
    // not 0's/empty strings)
    static ParamsStruct DefaultParamsAudio = {
        ndnlog::NdnLoggerDetailLevelNone,    // log level
        "",                   // log file
        true,   // use TLV encoding
        true,   // use RTX
        false,  // use FEC
        true,   // use memory cache
        true,   // use av sync
        true,   // use audio
        true,   // use video
        0,      // headless mode off
        
        0,      // capture device id
        0,      // capture width
        0,      // capture height
        0,      // capture framerate
        
        0,      // render width
        0,      // render height
        
        0,      // codec framerate
        0,      // gop
        0,      // codec start bitrate
        0,      // codec max bitrate
        0,      // codec encoding width
        0,      // codec encoding height
        0,      // drop frames
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "audio0",       // stream name
        "pcmu2",        // stream thread name
        "ndn/edu/ucla/apps",     // ndn hub
        1054,   // segment size for media frame
        5,      // data freshness (seconds) value
        50,     // producer rate (currently equal to playback rate)
        false,  // skip incomplete frames
        
        50,     // playback rate of local consumer
        2000,   // interest timeout
        90,     // assembling buffer size
        1054,   // frame buffer slot size
        200     // jitter buffer size in ms
    };

}

#endif
