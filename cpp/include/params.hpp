//
//  NdnParams.hpp
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

#include "ndnrtc-defines.hpp"

namespace ndnlog {
    typedef enum _NdnLoggerLevel {
        NdnLoggerLevelTrace = 0,
        NdnLoggerLevelDebug = 1,
        NdnLoggerLevelStat = 2,
        NdnLoggerLevelInfo = 3,
        NdnLoggerLevelWarning = 4,
        NdnLoggerLevelError = 5
    } NdnLoggerLevel;
    
    typedef enum _NdnLoggerDetailLevel {
        NdnLoggerDetailLevelNone = NdnLoggerLevelError+1,
        NdnLoggerDetailLevelDefault = NdnLoggerLevelInfo,
        NdnLoggerDetailLevelStat = NdnLoggerLevelStat,
        NdnLoggerDetailLevelDebug = NdnLoggerLevelDebug,
        NdnLoggerDetailLevelAll = NdnLoggerLevelTrace
    } NdnLoggerDetailLevel;
}

namespace ndnrtc
{
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
    
    // audio capture device parameters
    class AudioCaptureParams : public CaptureDeviceParams {
    };
    
    // frame segments metainformation
    // average number of segments per frame type
    // (available for video frames only)
    typedef struct _FrameSegmentsInfo {
        double deltaAvgSegNum_, deltaAvgParitySegNum_;
        double keyAvgSegNum_, keyAvgParitySegNum_;
        
        _FrameSegmentsInfo():deltaAvgSegNum_(0), deltaAvgParitySegNum_(0), keyAvgSegNum_(0),keyAvgParitySegNum_(0){}
        _FrameSegmentsInfo(double d, double dp, double k, double kp):deltaAvgSegNum_(d),
        deltaAvgParitySegNum_(dp), keyAvgSegNum_(k),keyAvgParitySegNum_(kp){}
        
        bool operator==(const _FrameSegmentsInfo& rhs) const
        {
            return this->deltaAvgSegNum_ == rhs.deltaAvgSegNum_ &&
            this->deltaAvgParitySegNum_ == rhs.deltaAvgParitySegNum_ &&
            this->keyAvgSegNum_ == rhs.keyAvgSegNum_ &&
            this->keyAvgParitySegNum_ == rhs.keyAvgParitySegNum_;
        }
        
        bool operator!=(const _FrameSegmentsInfo& rhs) const
        {
            return !(*this == rhs);
        }
    } FrameSegmentsInfo;
    
    // media thread parameters
    class MediaThreadParams : public Params {
    public:
        MediaThreadParams(){}
        MediaThreadParams(std::string threadName):threadName_(threadName){}
        MediaThreadParams(std::string threadName, FrameSegmentsInfo segInfo):
        threadName_(threadName), segInfo_(segInfo){}
        virtual ~MediaThreadParams(){}
        
        std::string threadName_ = "";
        FrameSegmentsInfo segInfo_;
        
        virtual void write(std::ostream& os) const
        {
            os << "name: " << threadName_;
        }
        
        virtual MediaThreadParams* copy() const = 0;
        
        FrameSegmentsInfo
        getSegmentsInfo()
        { return segInfo_; }
    };
    
    // audio thread parameters
    class AudioThreadParams : public MediaThreadParams {
    public:
        AudioThreadParams():MediaThreadParams("", FrameSegmentsInfo(1., 0., 0., 0.)),
        codec_("g722"){}
        AudioThreadParams(std::string threadName):MediaThreadParams(threadName, FrameSegmentsInfo(1., 0., 0., 0.)),
        codec_("g722"){}
        AudioThreadParams(std::string threadName, std::string codec):MediaThreadParams(threadName, FrameSegmentsInfo(1., 0., 0., 0.)),
        codec_(codec){}
        
        std::string codec_; // "g722" (SD) or "opus" (HD)
        
        MediaThreadParams*
        copy() const
        {
            AudioThreadParams *params = new AudioThreadParams();
            *params = *this;
            return params;
        }
        
        virtual void write(std::ostream& os) const
        {
            MediaThreadParams::write(os);
            if (threadName_.size()) os << "; codec: " << codec_;
        }
    };
    
    // video thread parameteres
    class VideoCoderParams : public Params {
    public:
        double codecFrameRate_;
        unsigned int gop_;
        unsigned int startBitrate_, maxBitrate_;
        unsigned int encodeWidth_, encodeHeight_;
        bool dropFramesOn_;
        
        VideoCoderParams():codecFrameRate_(0),gop_(0),startBitrate_(0),
        maxBitrate_(0),encodeWidth_(0),encodeHeight_(0),dropFramesOn_(false){}
        
        void write(std::ostream& os) const
        {
            os
            << codecFrameRate_ << "FPS; GOP: "
            << gop_ << "; Start bitrate: "
            << startBitrate_ << " Kbit/s; Max bitrate: "
            << maxBitrate_ << " Kbit/s; "
            << encodeWidth_ << "x" << encodeHeight_ << "; Drop: "
            << (dropFramesOn_?"YES":"NO");
        }
        
        bool operator==(const VideoCoderParams& rhs) const
        {
            return this->codecFrameRate_ == rhs.codecFrameRate_ &&
            this->gop_ == rhs.gop_ &&
            this->startBitrate_ == rhs.startBitrate_ &&
            this->maxBitrate_ == rhs.maxBitrate_ &&
            this->encodeWidth_ == rhs.encodeWidth_ &&
            this->encodeHeight_ == rhs.encodeHeight_ &&
            this->dropFramesOn_ == rhs.dropFramesOn_;
        }
        
        bool operator!=(const VideoCoderParams& rhs) const
        {
            return !(*this == rhs);
        }
    };
    
    class VideoThreadParams : public MediaThreadParams {
    public:
        VideoThreadParams():MediaThreadParams(){}
        VideoThreadParams(std::string threadName):MediaThreadParams(threadName){}
        VideoThreadParams(std::string threadName, FrameSegmentsInfo segInfo):
        MediaThreadParams(threadName, segInfo){}
        VideoThreadParams(std::string threadName, VideoCoderParams vcp):
        MediaThreadParams(threadName),coderParams_(vcp){}
        VideoThreadParams(std::string threadName, FrameSegmentsInfo segInfo, VideoCoderParams vcp):
        MediaThreadParams(threadName, segInfo), coderParams_(vcp){}
        
        VideoCoderParams coderParams_;
        
        void write(std::ostream& os) const
        {
            MediaThreadParams::write(os);
            
            os << "; " << coderParams_;
        }
        
        MediaThreadParams*
        copy() const
        {
            VideoThreadParams *params = new VideoThreadParams();
            *params = *this;
            return params;
        }
    };
    
    // general producer parameters
    class GeneralProducerParams : public Params {
    public:
        typedef struct _FreshnessPeriodParams {
            unsigned int metadataMs_;
            unsigned int sampleMs_;
            unsigned int sampleKeyMs_;
        } FreshnessPeriodParams;

        unsigned int segmentSize_ = 0;
        FreshnessPeriodParams freshness_ = {0, 0, 0};
        
        void write(std::ostream& os) const
        {
            os << "seg size: " << segmentSize_
               << " bytes; freshness (ms): metadata " << freshness_.metadataMs_ 
               << " sample " << freshness_.sampleMs_
               << " sample (key) " << freshness_.sampleKeyMs_;
        }
    };
    
    // media stream parameters
    class MediaStreamParams : public Params {
    public:
        typedef enum _MediaStreamType {
            MediaStreamTypeUnknown = -1,
            MediaStreamTypeAudio = 0,
            MediaStreamTypeVideo = 1,
            MediaStreamTypeData = 2
        } MediaStreamType;
        
        MediaStreamParams(){}
        MediaStreamParams(const std::string& name):streamName_(name){}
        MediaStreamParams(const MediaStreamParams& other)
        {
            copyFrom(other);
        }
        ~MediaStreamParams()
        {
            freeThreads();
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
        CaptureDeviceParams captureDevice_;
        MediaStreamType type_ = MediaStreamTypeAudio;
        
        size_t getThreadNum() const { return mediaThreads_.size(); }
        void addMediaThread(const AudioThreadParams& tp)
        { if (type_ == MediaStreamTypeAudio) addMediaThread(tp.copy()); }
        void addMediaThread(const VideoThreadParams& tp)
        { if (type_ == MediaStreamTypeVideo) addMediaThread(tp.copy()); }
        MediaThreadParams* getMediaThread(const unsigned int& idx) const
        { return (idx >= mediaThreads_.size()?nullptr:mediaThreads_[idx]); }
        AudioThreadParams* getAudioThread(const unsigned int& idx) const
        { return (type_ == MediaStreamTypeAudio?(AudioThreadParams*)getMediaThread(idx) : nullptr ); }
        VideoThreadParams* getVideoThread(const unsigned int& idx) const
        { return (type_ == MediaStreamTypeVideo?(VideoThreadParams*)getMediaThread(idx) : nullptr);}
        
        void write(std::ostream& os) const
        {
            os << "name: " << streamName_ << (type_ == MediaStreamTypeAudio ? " (audio)":" (video)")
            << "; synced to: " << synchronizedStreamName_
            << "; " << producerParams_ << "; ";
            
            if (captureDevice_.deviceId_ >= 0)
                os << "capture device " << captureDevice_;
            else
                os << "no device";
            
            os << "; " << mediaThreads_.size() << " threads:" << std::endl;
            
            for (int i = 0; i < mediaThreads_.size(); i++)
                os << "[" << i << ": " << *(mediaThreads_[i]) << "]" << std::endl;
        }
        
    private:
        std::vector<MediaThreadParams*> mediaThreads_;
        
        void
        copyFrom(const MediaStreamParams& other)
        {
            streamName_ = other.streamName_;
            type_ = other.type_;
            producerParams_ = other.producerParams_;
            synchronizedStreamName_ = other.synchronizedStreamName_;
            freeThreads();
            
            for (int i = 0; i < other.mediaThreads_.size(); i++)
                mediaThreads_.push_back(other.mediaThreads_[i]->copy());
            
            captureDevice_ = other.captureDevice_;
        }
        
        void
        freeThreads()
        {
            for (int i = 0; i < mediaThreads_.size(); i++)
                delete mediaThreads_[i];
            
            mediaThreads_.clear();
        }
        
        void addMediaThread(MediaThreadParams* const mtp)
        {
            mediaThreads_.push_back(mtp);
        }
    };
    
    // general consumer parameters
    class GeneralConsumerParams : public Params {
    public:
        unsigned int interestLifetime_ = 0;
        unsigned int jitterSizeMs_ = 0;
        
        void write(std::ostream& os) const
        {
            os << "interest lifetime: " << interestLifetime_
            << " ms; jitter size: " << jitterSizeMs_
            << " ms";
        }
    };
    
    // general app-wide parameters
    class GeneralParams : public Params {
    public:
        // general
        ndnlog::NdnLoggerDetailLevel loggingLevel_;
        std::string logFile_;
        std::string logPath_;
        bool useFec_, useAvSync_;
        
        // network
        std::string host_;
        unsigned int portNum_;
        
        GeneralParams():loggingLevel_(ndnlog::NdnLoggerDetailLevelNone),
        useFec_(false), useAvSync_(false), portNum_(6363){}
        
        void write(std::ostream& os) const
        {
#if defined __APPLE__
            static std::string lvlToString[] = {
                [ndnlog::NdnLoggerDetailLevelNone] = "NONE",
                [ndnlog::NdnLoggerLevelTrace] =      "TRACE",
                [ndnlog::NdnLoggerLevelDebug] =      "DEBUG",
                [ndnlog::NdnLoggerLevelInfo] =       "INFO",
                [ndnlog::NdnLoggerLevelWarning] =    "WARN",
                [ndnlog::NdnLoggerLevelError] =      "ERROR",
                [ndnlog::NdnLoggerLevelStat] =       "STAT"
            };
#else
            static std::string lvlToString[] = {
                [0] = "TRACE",
                [1] = "DEBUG",
                [2] = "STAT",
                [3] = "INFO",
                [4] = "WARN",
                [5] = "ERROR",
                [6] = "NONE"
            };
#endif
            os << "log level: " << lvlToString[loggingLevel_]
            << "; log file: " << logFile_ << " (at " << logPath_ << ")"
            << "; FEC: " << (useFec_?"ON":"OFF")
            << "; A/V Sync: " << (useAvSync_?"ON":"OFF")
            << "; Host: " << host_
            << "; Port #: " << portNum_;
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

#endif
