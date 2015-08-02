//
//  media-stream.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __libndnrtc__media_stream__
#define __libndnrtc__media_stream__

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "params.h"
#include "ndnrtc-common.h"
#include "media-thread.h"
#include "face-wrapper.h"
#include "external-capturer.hpp"
#include "audio-capturer.h"

namespace ndnrtc
{
    namespace new_api {
        
        class MediaStreamSettings
        {
        public:
            MediaStreamSettings(){}
            MediaStreamSettings(const MediaStreamParams& params):streamParams_(params){}
            ~MediaStreamSettings(){}
            
            bool useFec_;
            std::string userPrefix_;
            MediaStreamParams streamParams_;
            boost::shared_ptr<KeyChain> keyChain_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
            boost::shared_ptr<MemoryContentCache> memoryCache_;
        };
        
        class IMediaStreamCallback : public IMediaThreadCallback
        {
        public:
            
        };
        
        class MediaStream : public NdnRtcComponent
        {
        public:
            MediaStream();
            virtual ~MediaStream();
            
            virtual int
            init(const MediaStreamSettings& streamSettings);
            
            virtual void
            release() = 0;
            
            virtual void
            addThread(const MediaThreadParams& threadParams);
            
            virtual void
            removeThread(const std::string& threadName);
            
            virtual void
            removeAllThreads();
            
            MediaStreamSettings
            getSettings()
            { return settings_; }
            
            std::string
            getPrefix()
            { return streamPrefix_; }
            
            virtual MediaStreamParams
            getStreamParameters() = 0;
            
        protected:
            MediaStreamSettings settings_;
            std::string streamName_;
            std::string streamPrefix_;
            typedef std::map<std::string, boost::shared_ptr<MediaThread>> ThreadMap;
            ThreadMap threads_;
            
            void
            getCommonThreadSettings(MediaThreadSettings* settings);
            
            virtual int
            addNewMediaThread(const MediaThreadParams* params) = 0;
            
            void
            onMediaThreadRegistrationFailed(std::string threadName);
        };
        
        //**********************************************************************
        class VideoStream : public MediaStream,
                            public IRawFrameConsumer
        {
        public:
            VideoStream();
            
            int
            init(const MediaStreamSettings& streamSettings);
            
            void
            release();
            
            ExternalCapturer*
            getCapturer() { return (ExternalCapturer*)(capturer_.get()); }
            
            MediaStreamParams
            getStreamParameters();
            
        private:
            bool isProcessing_;
            boost::shared_ptr<BaseCapturer> capturer_;
            
            int
            addNewMediaThread(const MediaThreadParams* params);
            
            // IRawFrameConsumer
            void
            onDeliverFrame(WebRtcVideoFrame &frame,
                           double timestamp);
        };
        
        //**********************************************************************
        class AudioStream : public MediaStream,
                            public IAudioFrameConsumer
        {
        public:
            AudioStream();
            
            int
            init(const MediaStreamSettings& streamSettings);
            
            void
            release();
            
            MediaStreamParams
            getStreamParameters()
            { return MediaStreamParams(settings_.streamParams_); }
            
        private:
            boost::shared_ptr<AudioCapturer> audioCapturer_;
            
            int
            addNewMediaThread(const MediaThreadParams* params);
            
            // IAudioFrameConsumer
            void
            onDeliverRtpFrame(unsigned int len, unsigned char *data);
            
            void
            onDeliverRtcpFrame(unsigned int len, unsigned char *data);
        };
    }
}

#endif /* defined(__libndnrtc__media_stream__) */
