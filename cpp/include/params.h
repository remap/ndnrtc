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
#include <vector>

#include "simple-log.h"
#include "ndnrtc-defines.h"

#define PARAM_OK(p, l, r) (p >= l && p <= r)

namespace ndnrtc
{
    namespace new_api {
        
        class Params {
        public:
            virtual ~Params(){}
            virtual void write(std::ostream& os) const {}
            
            friend std::ostream& operator<<(std::ostream& os, Params const& p)
            {
                p.write(os);
                return os;
            }
        };
        
        // capture device parameters
        class CaptureDeviceParams : public Params {
        public:
            ~CaptureDeviceParams(){}
            
            int deviceId_ = -2;
            
            virtual void write(std::ostream& os) const
            {
                os << "id: " << deviceId_;
            }
        };
                
        // video capture device parameters
        class VideoCaptureParams : public CaptureDeviceParams {
        public:
            unsigned int captureWidth_ = 0, captureHeight_ = 0;
            double framerate_ = 0;
            
            void write(std::ostream& os) const
            {
                CaptureDeviceParams::write(os);
                
                os << "; " << framerate_ << "FPS; "
                << captureWidth_ << "x" << captureHeight_;
            }
        };
        
        // audio capture device parameters
        class AudioCaptureParams : public CaptureDeviceParams {
        };
        
        // media thread parameters
        class MediaThreadParams : public Params {
        public:
            ~MediaThreadParams(){}
            
            std::string threadName_ = "";
            
            virtual void write(std::ostream& os) const
            {
                os << "name: " << threadName_;
            }
        };
        
        // audio thread parameters
        class AudioThreadParams : public MediaThreadParams {
        public:
        };
        
        // video thread parameteres
        class VideoThreadParams : public MediaThreadParams {
        public:
            double codecFrameRate_ = 0;
            unsigned int gop_ = 0;
            unsigned int startBitrate_ = 0, maxBitrate_ = 0;
            unsigned int encodeWidth_ = 0, encodeHeight_ = 0;
            bool dropFramesOn_ = false;
            
            void write(std::ostream& os) const
            {
                MediaThreadParams::write(os);
                
                os << "; "
                << codecFrameRate_ << "FPS; GOP: "
                << gop_ << "; Start bitrate: "
                << startBitrate_ << "kbit/s; Max bitrate:"
                << maxBitrate_ << "kbit/s; "
                << encodeWidth_ << "x" << encodeHeight_ << "; Drop: "
                << (dropFramesOn_?"YES":"NO");
            }
        };
        
        // media stream parameters
        class MediaStreamParams : public Params {
        public:
            ~MediaStreamParams()
            {
                for (int i = 0; i < mediaThreads_.size(); i++) delete mediaThreads_[i];
                mediaThreads_.clear();
            }
            
            std::string streamName_ = "";
            CaptureDeviceParams *captureDevice_ = NULL;
            std::vector<MediaThreadParams*> mediaThreads_;
            
            void write(std::ostream& os) const
            {
                os << "name: " << streamName_ << "; ";
                
                if (captureDevice_)
                    os << *captureDevice_;
                else
                    os << "no device";
                
                os << "; " << mediaThreads_.size() << " threads: " << std::endl;
                
                for (int i = 0; i < mediaThreads_.size(); i++)
                    os << "[" << i << ": " << *(mediaThreads_[i]) << "]" << std::endl;
            }
            
        };
        
        // general producer parameters
        class GeneralProducerParams : public Params {
        public:
            unsigned int segmentSize_ = 0, freshnessMs_ = 0;
            
            void write(std::ostream& os) const
            {
                os << "seg size: " << segmentSize_
                << "; freshness: " << freshnessMs_;
            }
        };
        
        // general consumer parameters
        class GeneralConsumerParams : public Params {
        public:
            unsigned int interestLifetime_ = 0;
            unsigned int bufferSlotsNum_ = 0, slotSize_ = 0;
            unsigned int jitterSizeMs_ = 0;
            
            void write(std::ostream& os) const
            {
                os << "interest lifetime: " << interestLifetime_
                << "; jitter size: " << jitterSizeMs_ << "ms"
                << "; buffer size (slots): " << bufferSlotsNum_
                << "; slot size: " << slotSize_;
            }
        };
        
        // headless mode parameters
        class HeadlessModeParams : public Params {
        public:
            typedef enum _HeadlessMode {
                HeadlessModeOff = 0,
                HeadlessModeConsumer = 1,
                HeadlessModeProducer = 2
            } HeadlessMode;
            
            HeadlessMode mode_ = HeadlessModeOff;
            std::string username_ = "";
            
            void write(std::ostream& os) const
            {
                os << "headless mode: " << (mode_ == HeadlessModeParams::HeadlessModeOff?"OFF":(mode_ == HeadlessModeParams::HeadlessModeConsumer?"CONSUMER":"PRODUCER"))
                << "; username: " << username_;
            }
        };
        
        // general app-wide parameters
        class GeneralParams : public Params {
        public:
            // general
            ndnlog::NdnLoggerDetailLevel loggingLevel_ = ndnlog::NdnLoggerDetailLevelNone;
            std::string logFile_ = "";
            bool
            useTlv_ = false,
            useRtx_ = false,
            useFec_ = false,
            useCache_ = false,
            useAudio_ = false,
            useVideo_ = false,
            useAvSync_ = false,
            skipIncomplete_ = false;
            
            // network
            std::string prefix_ = "", host_ = "";
            unsigned int portNum_ = 0;
            
            void write(std::ostream& os) const
            {
                os << "log level: " << ndnlog::new_api::Logger::stringify((ndnlog::NdnLoggerLevel)loggingLevel_)
                << "; log file: " << logFile_
                << "; TLV: " << (useTlv_?"ON":"OFF")
                << "; RTX: " << (useRtx_?"ON":"OFF")
                << "; FEC: " << (useFec_?"ON":"OFF")
                << "; Cache: " << (useCache_?"ON":"OFF")
                << "; Audio: " << (useAudio_?"ON":"OFF")
                << "; Video: " << (useVideo_?"ON":"OFF")
                << "; A/V Sync: " << (useAvSync_?"ON":"OFF")
                << "; Skipping incomplete frames: " << (skipIncomplete_?"ON":"OFF")
                << "; Prefix: " << prefix_
                << "; Host: " << host_
                << "; Port #: " << portNum_;
            }
        };
        
        // application parameters
        // configuration file should be read into this structure
        class AppParams : public Params {
        public:
            AppParams(){}
            ~AppParams()
            {
                for (int i = 0; i < audioDevices_.size(); i++) delete audioDevices_[i];
                audioDevices_.clear();
                for (int i = 0; i < videoDevices_.size(); i++) delete videoDevices_[i];
                videoDevices_.clear();
                for (int i = 0; i < defaultAudioStreams_.size(); i++) delete defaultAudioStreams_[i];
                defaultAudioStreams_.clear();
                for (int i = 0; i < defaultVideoStreams_.size(); i++) delete defaultVideoStreams_[i];
                defaultVideoStreams_.clear();
            }
            
            GeneralParams generalParams_;
            HeadlessModeParams headlessModeParams_;
            
            // producer general params
            GeneralProducerParams
            videoProducerParams_,
            audioProducerParams_;
            
            // consumer general params
            GeneralConsumerParams
            videoConsumerParams_,
            audioConsumerParams_;

            // capture devices
            std::vector<CaptureDeviceParams*>
            audioDevices_,
            videoDevices_;
            
            // default media streams
            std::vector<MediaStreamParams*>
            defaultAudioStreams_,
            defaultVideoStreams_;

            void write(std::ostream& os) const
            {
                os
                << "-General:" << std::endl
                << generalParams_ << std::endl
                << "-Headless mode: " << headlessModeParams_ << std::endl
                << "-Producer:" << std::endl
                << "--Audio: " << audioProducerParams_ << std::endl
                << "--Video: " << videoProducerParams_ << std::endl
                << "-Consumer:" << std::endl
                << "--Audio: " << audioConsumerParams_ << std::endl
                << "--Video: " << videoConsumerParams_ << std::endl
                << "-Capture devices:" << std::endl
                << "--Audio:" << std::endl;
                
                for (int i = 0; i < audioDevices_.size(); i++)
                    os << i << ": " << *audioDevices_[i] << std::endl;
                
                os << "--Video:" << std::endl;
                for (int i = 0; i < videoDevices_.size(); i++)
                    os << i << ": " << *videoDevices_[i] << std::endl;
                
                os << "-Producer streams:" << std::endl
                << "--Audio:" << std::endl;
                
                for (int i = 0; i < defaultAudioStreams_.size(); i++)
                    os << i << ": " << *defaultAudioStreams_[i] << std::endl;
                
                os << "--Video:" << std::endl;
                for (int i = 0; i < defaultVideoStreams_.size(); i++)
                    os << i << ": " << *defaultVideoStreams_[i] << std::endl;
                
            }
        };
    }
    
    static const unsigned int
        MaxWidth = 5000,
        MinWidth = 100,
        MaxHeight = 5000,
        MinHeight = 100,
        MaxPortNum = 65535,
        MaxStartBitrate = 50000,
        MinStartBitrate = 0,
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
    
    typedef struct _CodecParams {
        unsigned int idx;
        double codecFrameRate;
        unsigned int gop;
        unsigned int startBitrate, maxBitrate;
        unsigned int encodeWidth, encodeHeight;
        bool dropFramesOn;
    } CodecParams;

    typedef struct _ParamsStruct {
        ndnlog::NdnLoggerDetailLevel loggingLevel;
        const char *logFile;
        bool useTlv, useRtx, useFec, useCache, useAudio, useVideo, useAvSync;
        unsigned int headlessMode; // 0 - off, 1 - consumer, 2 - producer
        
        // capture settings
        unsigned int captureDeviceId;
        unsigned int captureWidth, captureHeight;
        unsigned int captureFramerate;
        
        // render
        unsigned int renderWidth, renderHeight;
        
        // streams
        unsigned int nStreams;
        CodecParams* streamsParams;
        
        // network parameters
        const char *host;
        unsigned int portNum;
        
        // ndn producer
        const char *producerId;
        const char *streamName;
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
        
        void addNewStream(const CodecParams& streamParams)
        {
            this->nStreams++;
            this->streamsParams = (CodecParams*)realloc(this->streamsParams,
                                                         this->nStreams*sizeof(CodecParams));
            this->streamsParams[this->nStreams-1] = streamParams;
        }
        
    } ParamsStruct;
    
    static CodecParams DefaultCodecParams = {
        0,      // idx
        30,     // fps
        30,     // gop
        300,    // start bitrate
        0,      // max bitrate
        640,    // encode width
        480     // encode height
    };
    
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
        
        0,      // number of simultaneous streams
        NULL, // no streams
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "video0",       // stream name
        "ndn/edu/ucla/remap",     // ndn hub
        1000,   // segment size for media frame (MTU - NDN header (currently 446 bytes))
        1,      // data freshness (seconds) value
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
        
        0,
        NULL,
        
        "localhost",    // network ndnd remote host
        6363,           // default ndnd port number
        
        "testuser",     // producer id
        "audio0",       // stream name
        "ndn/edu/ucla/remap",     // ndn hub
        1000,   // segment size for media frame
        1,      // data freshness (seconds) value
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
