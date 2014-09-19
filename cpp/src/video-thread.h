//
//  video-thread.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_thread__
#define __ndnrtc__video_thread__

#include "ndnrtc-common.h"
#include "ndnrtc-namespace.h"
#include "video-coder.h"
#include "frame-buffer.h"
#include "media-thread.h"

namespace ndnrtc
{
    namespace new_api
    {
        class VideoThreadSettings : public MediaThreadSettings
        {
        public:
            bool useFec_;
            
            VideoThreadParams*
            getVideoParams()
            { return (VideoThreadParams*)&threadParams_;}
        };
        
        class VideoThreadStatistics : public MediaThreadStatistics
        {
        public:
            VideoCoderStatistics coderStatistics_;
            unsigned int nKeyFramesPublished_, nDeltaFramesPublished_;
        };
        
        /**
         * This class is a subclass of MediaThread for video streams
         */
        class VideoThread : public MediaThread,
                            public IRawFrameConsumer,
                            public IEncodedFrameConsumer
        {
        public:
            VideoThread();
            ~VideoThread(){}
            
            static const double ParityRatio;
            
            // overriden from base class
            int init(const VideoThreadSettings& settings);
            
            void
            getStatistics(VideoThreadStatistics& statistics)
            {
                MediaThread::getStatistics(statistics);
                coder_->getStatistics(statistics.coderStatistics_);
                statistics.nKeyFramesPublished_ = keyFrameNo_;
                statistics.nDeltaFramesPublished_ = deltaFrameNo_;
            }
            
            void
            getSettings(VideoThreadSettings& settings)
            {
                coder_->getSettings(settings_.getVideoParams()->coderParams_);
                settings = settings_;
            }
            
            // interface conformance
            void onDeliverFrame(webrtc::I420VideoFrame &frame,
                                double unixTimeStamp);
            
            void setLogger(ndnlog::new_api::Logger *logger);
            
        private:
            VideoThreadSettings settings_;
            int keyFrameNo_ = 0, deltaFrameNo_ = 0;
            
            Name deltaFramesPrefix_, keyFramesPrefix_;
            boost::shared_ptr<VideoCoder> coder_;
            
            void onInterest(const boost::shared_ptr<const Name>& prefix,
                            const boost::shared_ptr<const Interest>& interest,
                            ndn::Transport& transport);
            
            int
            publishParityData(PacketNumber frameNo,
                              const PacketData& packetData,
                              unsigned int nSegments,
                              const Name& framePrefix,
                              const PrefixMetaInfo& prefixMeta);
            
            // interface conformance
            void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                         double captureTimestamp);
        };
    }
}

#endif /* defined(__ndnrtc__video_thread__) */
