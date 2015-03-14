//
//  video-playout.h
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__video_playout__
#define __ndnrtc__video_playout__

#include "playout.h"
#include "video-coder.h"

namespace ndnrtc {
    namespace new_api {
        class VideoConsumer;
        
        typedef boost::function<void (PacketNumber, PacketNumber, PacketNumber, bool, double)> OnFrameSkipped;
        
        class VideoPlayout : public Playout
        {
        public:
            VideoPlayout(Consumer* consumer,
                         const boost::shared_ptr<statistics::StatisticsStorage>& statStorage);
            ~VideoPlayout();
            
            int
            start(int playbackAdjustment = 0);
            
            OnFrameSkipped onFrameSkipped_;
            
        private:
            // this flags indicates whether frames should be played out (unless
            // new full key frame received)
            bool validGop_;
            PacketNumber currentKeyNo_;
            
            bool
            playbackPacket(int64_t packetTsLocal, PacketData* data,
                           PacketNumber playbackPacketNo,
                           PacketNumber sequencePacketNo,
                           PacketNumber pairedPacketNo,
                           bool isKey, double assembledLevel);
            
            VideoConsumer*
            getVideoConsumer()
            { return (VideoConsumer*)consumer_; }
        };
    }
}

#endif /* defined(__ndnrtc__video_playout__) */
