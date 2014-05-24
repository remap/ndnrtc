//
//  audio-playout.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__audio_playout__
#define __ndnrtc__audio_playout__

#include "ndnrtc-common.h"
#include "playout.h"
#include "audio-renderer.h"

namespace ndnrtc {
    namespace new_api {
        class AudioPlayout : public Playout
        {
        public:
            AudioPlayout(const Consumer* consumer);
            ~AudioPlayout();
            
        private:
            bool
            playbackPacket(int64_t packetTsLocal, PacketData* data,
                           PacketNumber playbackPacketNo,
                           PacketNumber sequencePacketNo,
                           PacketNumber pairedPacketNo,
                           bool isKey, double assembledLevel);
        };
    }
}

#endif /* defined(__ndnrtc__audio_playout__) */
