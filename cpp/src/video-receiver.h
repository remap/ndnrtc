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

#include "ndnrtc-common.h"

namespace ndnrtc
{
    class NdnVideoReceiver {
    private:
//        shared_ptr<Face> face_;
//        // buffer for assemnbling frames
//        std::vector<void*> assemblingBuffer_;
//        // playout buffer contains ready-to-decode-and-play frames
//        std::vector<webrtc::EncodedImage> playoutBuffer_;
//        
//        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> playoutBuffer_cs_;
//        webrtc::ThreadWrapper &playoutThread, pipelineThread_;
//        webrtc::EventWrapper &frameAssembledEvent_;
//        
//        // ndn-lib callbacks
//        void onTimeout(const shared_ptr<const Interest>& interest);
//        void onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data);
//        
//        // needed to receive callbacks from ndn-lib
//        bool processNetwork();
//        // pipelines interests for upcoming frames' segments
//        bool pipelineInterests();
//        // supplies consumer with assembled encoded frames
//        bool supplyFrames();
        
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
