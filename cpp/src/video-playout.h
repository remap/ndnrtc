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

namespace ndnrtc {
    namespace new_api {
        class VideoPlayout : public Playout
        {
        public:
            VideoPlayout(const shared_ptr<const Consumer>& consumer);
            ~VideoPlayout();
            
        private:
            bool
            playbackPacket(int64_t packetTsLocal);
        };
    }
}

#endif /* defined(__ndnrtc__video_playout__) */
