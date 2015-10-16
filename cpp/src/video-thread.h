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

#include <boost/thread/mutex.hpp>

#include "ndnrtc-common.h"
#include "ndnrtc-namespace.h"
#include "video-coder.h"
#include "frame-buffer.h"
#include "media-thread.h"
#include "threading-capability.h"

namespace ndnrtc
{
    namespace new_api
    {
        class VideoThreadSettings : public MediaThreadSettings
        {
        public:
            bool useFec_;
            
            VideoThreadParams*
            getVideoParams() const
            { return (VideoThreadParams*)threadParams_;}
        };
        
        class VideoThreadStatistics : public MediaThreadStatistics
        {
        public:
            VideoCoderStatistics coderStatistics_;
            unsigned int nKeyFramesPublished_, nDeltaFramesPublished_;
            double deltaAvgSegNum_, keyAvgSegNum_;
            double deltaAvgParitySegNum_, keyAvgParitySegNum_;
        };
        
        /**
         * This class is a subclass of MediaThread for video streams
         */
        class VideoThread : public MediaThread,
                            public IRawFrameConsumer,
                            public IEncoderDelegate,
                            public ThreadingCapability
        {
        public:
            VideoThread();
            ~VideoThread();
            
            static const double ParityRatio;
            
            // overriden from base class
            int
            init(const VideoThreadSettings& settings);
            
            void
            getStatistics(VideoThreadStatistics& statistics);
            
            void
            getSettings(VideoThreadSettings& settings)
            {
                coder_->getSettings(getSettings().getVideoParams()->coderParams_);
                settings = getSettings();
            }
            
            // interface conformance
            void
            onDeliverFrame(WebRtcVideoFrame &frame,
                           double unixTimeStamp);
            
            void
            setLogger(ndnlog::new_api::Logger *logger);
            
            bool
            isThreadStatReady()
            { return keyFrameNo_ > 0 && deltaFrameNo_ > 0; }
            
        private:
            int keyFrameNo_ = 0, deltaFrameNo_ = 0, gopCount_ = 0;
            unsigned int deltaSegnumEstimatorId_, keySegnumEstimatorId_;
            unsigned int deltaParitySegnumEstimatorId_, keyParitySegnumEstimatorId_;
            int64_t encodingTimestampMs_;
            
            Name deltaFramesPrefix_, keyFramesPrefix_;
            boost::shared_ptr<VideoCoder> coder_;
            
            boost::mutex frameProcessingMutex_;
            WebRtcVideoFrame deliveredFrame_;
            
            void
            onInterest(const boost::shared_ptr<const Name>& prefix,
                       const boost::shared_ptr<const Interest>& interest,
                       ndn::Face& face,
                       uint64_t ts,
                       const boost::shared_ptr<const InterestFilter>& filter);
            
            int
            publishParityData(PacketNumber frameNo,
                              const PacketData& packetData,
                              unsigned int nSegments,
                              const Name& framePrefix,
                              const PrefixMetaInfo& prefixMeta);
            
            // interface conformance
            void
            onEncodingStarted();
            
            void
            onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                    double captureTimestamp,
                                    bool completeFrame);
            
            void
            onFrameDropped();
            
            void
            publishFrameData(const webrtc::EncodedImage &encodedImage,
                             double captureTimestamp,
                             bool completeFrame);
            
            VideoThreadSettings&
            getSettings() const
            { return *((VideoThreadSettings*)settings_); }
        };
    }
}

#endif /* defined(__ndnrtc__video_thread__) */
