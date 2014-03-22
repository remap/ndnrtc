//
//  playout.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#ifndef __ndnrtc__playout__
#define __ndnrtc__playout__

#include "ndnrtc-common.h"
#include "playout-buffer.h"
#include "consumer.h"
#include "frame-buffer.h"
#include "video-sender.h"

namespace ndnrtc{
    namespace new_api {
        
        class Playout : public ndnlog::new_api::ILoggingObject
        {
        public:
            Playout(const shared_ptr<const Consumer> &consumer);
            ~Playout();
            
            virtual int
            init(IEncodedFrameConsumer* frameConsumer);
            
            virtual int
            start();
            
            virtual int
            stop();
            
        protected:
            bool isRunning_;
            shared_ptr<const Consumer> consumer_;
            shared_ptr<FrameBuffer> frameBuffer_;
            
            JitterTiming jitterTiming_;
            webrtc::ThreadWrapper &playoutThread_;
            
            IEncodedFrameConsumer *frameConsumer_;
            PacketData *data_;
            
            virtual void
            playbackPacket(int64_t packetTsLocal) = 0;
            
            static bool
            playoutThreadRoutine(void *obj)
            {
                return ((Playout*)obj)->processPlayout();
            }
            
            bool
            processPlayout();
            
            virtual std::string
            getDescription() const
            { return "playout"; }
            
        };
    }
}

#endif /* defined(__ndnrtc__playout__) */
