//
//  video-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_receiver__
#define __ndnrtc__video_receiver__

#include "media-receiver.h"
#include "video-sender.h"
#include "video-playout-buffer.h"

namespace ndnrtc
{
    class AudioVideoSynchronizer;
    
    class NdnVideoReceiver :    public NdnMediaReceiver
    {
        friend class AudioVideoSynchronizer;
    public:
        NdnVideoReceiver(const ParamsStruct &params);
        ~NdnVideoReceiver();
        
        int startFetching();
        void setFrameConsumer(IEncodedFrameConsumer *consumer) {
            frameConsumer_ = consumer;
        }
        
    private:
        IEncodedFrameConsumer *frameConsumer_;
    
        // overriden
        void playbackPacket(int64_t packetTsLocal);
        void switchToMode(NdnVideoReceiver::ReceiverMode mode);
        bool isLate(const Name &prefix, const unsigned char *segmentData,
                    int dataSz);
        bool needMoreKeyFrames();
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
