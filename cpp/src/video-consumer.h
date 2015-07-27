//
//  video-consumer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_consumer__
#define __ndnrtc__video_consumer__

#include "consumer.h"
#include "video-renderer.h"
#include "video-decoder.h"
#include "statistics.h"

namespace ndnrtc {
    namespace new_api {
        class VideoConsumer : public Consumer,
                              public IInterestQueueCallback
        {
        public:
            VideoConsumer(const GeneralParams& generalParams,
                          const GeneralConsumerParams& consumerParams,
                          IExternalRenderer* const externalRenderer);
            virtual ~VideoConsumer();
            
            int
            init(const ConsumerSettings& settings,
                 const std::string& threadName);
            
            int
            start();
            
            int
            stop();
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
            void
            onInterestIssued(const boost::shared_ptr<const ndn::Interest>& interest);
            
            void
            onStateChanged(const int& oldState, const int& newState);
            
            /**
             * Called by video playout mechanism to notify consumer observer 
             * about new playback events
             */
            void
            playbackEventOccurred(PlaybackEvent event, unsigned int frameSeqNo);
            
            void
            triggerRebuffering();

        private:
            boost::shared_ptr<NdnVideoDecoder> decoder_;
            
            boost::shared_ptr<IVideoRenderer>
            getRenderer()
            { return boost::dynamic_pointer_cast<IVideoRenderer>(renderer_); }
            
            void
            onFrameSkipped(PacketNumber playbackNo, PacketNumber sequenceNo,
                           PacketNumber pairedNo, bool isKey,
                           double assembledLevel);
            
            void
            onTimeout(const boost::shared_ptr<const Interest>& interest);
        };
    }
}

#endif /* defined(__ndnrtc__video_consumer__) */
