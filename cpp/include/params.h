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
        
        // frame segments metainformation
        // average number of segments per frame type
        // (available for video frames only)
        typedef struct _FrameSegmentsInfo {
            double deltaAvgSegNum_, deltaAvgParitySegNum_;
            double keyAvgSegNum_, keyAvgParitySegNum_;
        } FrameSegmentsInfo;
        
        // media thread parameters
        class MediaThreadParams : public Params {
        public:
            virtual ~MediaThreadParams(){}
            
            std::string threadName_ = "";
            
            virtual void write(std::ostream& os) const
            {
                os << "name: " << threadName_;
            }
            
            virtual MediaThreadParams* copy()
            {
                MediaThreadParams *params = new MediaThreadParams();
                *params = *this;
                return params;
            }
            
            virtual FrameSegmentsInfo
            getSegmentsInfo()
            { return (FrameSegmentsInfo){0., 0., 0., 0.}; }
        };
        
        // audio thread parameters
        class AudioThreadParams : public MediaThreadParams {
        public:
            MediaThreadParams*
            copy()
            {
                AudioThreadParams *params = new AudioThreadParams();
                *params = *this;
                return params;
            }
            
            FrameSegmentsInfo
            getSegmentsInfo()
            { return (FrameSegmentsInfo){1., 0., 0., 0.}; }
        };
        
        // video thread parameteres
        class VideoCoderParams : public Params {
        public:
            double codecFrameRate_;
            unsigned int gop_;
            unsigned int startBitrate_, maxBitrate_;
            unsigned int encodeWidth_, encodeHeight_;
            bool dropFramesOn_;
            
            void write(std::ostream& os) const
            {
                os
                << codecFrameRate_ << "FPS; GOP: "
                << gop_ << "; Start bitrate: "
                << startBitrate_ << "kbit/s; Max bitrate:"
                << maxBitrate_ << "kbit/s; "
                << encodeWidth_ << "x" << encodeHeight_ << "; Drop: "
                << (dropFramesOn_?"YES":"NO");
            }
        };
        
        class VideoThreadParams : public MediaThreadParams {
        public:
            VideoCoderParams coderParams_;
            double deltaAvgSegNum_, deltaAvgParitySegNum_;
            double keyAvgSegNum_, keyAvgParitySegNum_;

            void write(std::ostream& os) const
            {
                MediaThreadParams::write(os);
                
                os << "; " << coderParams_;
            }
            MediaThreadParams*
            copy()
            {
                VideoThreadParams *params = new VideoThreadParams();
                *params = *this;
                return params;
            }
            
            FrameSegmentsInfo
            getSegmentsInfo()
            { return (FrameSegmentsInfo){deltaAvgSegNum_, deltaAvgParitySegNum_,
                keyAvgSegNum_, keyAvgParitySegNum_}; }
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
        
        // media stream parameters
        class MediaStreamParams : public Params {
        public:
            typedef enum _MediaStreamType {
                MediaStreamTypeAudio = 0,
                MediaStreamTypeVideo = 1
            } MediaStreamType;
            
            MediaStreamParams(){}
            MediaStreamParams(const MediaStreamParams& other)
            {
                copyFrom(other);
            }
            ~MediaStreamParams()
            {
                freeThreads();

                if (captureDevice_)
                    delete captureDevice_;
            }
            
            MediaStreamParams& operator=(const MediaStreamParams& other)
            {
                if (this == &other)
                    return *this;
                
                copyFrom(other);
                return *this;
            }
            
            GeneralProducerParams producerParams_;
            std::string streamName_ = "";
            std::string synchronizedStreamName_ = "";
            CaptureDeviceParams *captureDevice_ = NULL;
            std::vector<MediaThreadParams*> mediaThreads_;
            MediaStreamType type_;
            
            void write(std::ostream& os) const
            {
                os << "name: " << streamName_ << "; " << producerParams_ << "; ";
                
                if (captureDevice_)
                    os << *captureDevice_;
                else
                    os << "no device";
                
                os << "; " << mediaThreads_.size() << " threads: " << std::endl;
                
                for (int i = 0; i < mediaThreads_.size(); i++)
                    os << "[" << i << ": " << *(mediaThreads_[i]) << "]" << std::endl;
            }
            
        private:
            void
            copyFrom(const MediaStreamParams& other)
            {
                streamName_ = other.streamName_;
                type_ = other.type_;
                producerParams_ = other.producerParams_;
                freeThreads();
                
                for (int i = 0; i < other.mediaThreads_.size(); i++)
                    mediaThreads_.push_back(other.mediaThreads_[i]->copy());
                
                if (other.captureDevice_)
                {
                    captureDevice_ = new CaptureDeviceParams();
                    *captureDevice_ = *other.captureDevice_;
                }
            }
            
            void
            freeThreads()
            {
                for (int i = 0; i < mediaThreads_.size(); i++)
                    delete mediaThreads_[i];
                
                mediaThreads_.clear();
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
            std::string logPath_ = "";
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
        
        class SessionInfo : public Params {
        public:
            SessionInfo(){}
            SessionInfo(const SessionInfo& other)
            { copyFrom(other); }
            ~SessionInfo()
            {
                freeAudioStreams();
                freeVideoStreams();
            }
            SessionInfo& operator=(const SessionInfo& other)
            {
                if (this == &other)
                    return *this;
                
                copyFrom(other);
                return *this;
            }
            
            std::string sessionPrefix_;
            std::vector<MediaStreamParams*> audioStreams_;
            std::vector<MediaStreamParams*> videoStreams_;
            
            void write(std::ostream& os) const
            {
                os << "audio streams: " << std::endl;
                for (int i = 0; i < audioStreams_.size(); i++)
                    os << i << ": " << *audioStreams_[i] << std::endl;
                    
                os << "video streams:" << std::endl;
                for (int i = 0; i < videoStreams_.size(); i++)
                    os << i << ": " << *videoStreams_[i] << std::endl;
            }
            
        private:
            void
            copyFrom(const SessionInfo& other)
            {
                sessionPrefix_ = other.sessionPrefix_;
                freeAudioStreams();
                freeVideoStreams();
                
                for (int i = 0; i < other.audioStreams_.size(); i++)
                {
                    MediaStreamParams* params = new MediaStreamParams(*other.audioStreams_[i]);
                    audioStreams_.push_back(params);
                }
                
                for (int i = 0; i < other.videoStreams_.size(); i++)
                {
                    MediaStreamParams* params = new MediaStreamParams(*other.videoStreams_[i]);
                    videoStreams_.push_back(params);
                }
            }
            
            void
            freeAudioStreams()
            {
                for (int i = 0; i < audioStreams_.size(); i++)
                    delete audioStreams_[i];
                audioStreams_.clear();
            }
            
            void
            freeVideoStreams()
            {
                for (int i = 0; i < videoStreams_.size(); i++)
                    delete videoStreams_[i];
                videoStreams_.clear();
                
            }
        };
    }
}

#endif
