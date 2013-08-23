//
//  video-sender.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__video_sender__
#define __ndnrtc__video_sender__

#include "ndnrtc-common.h"
#include "video-coder.h"

namespace ndnrtc
{
    class INdnVideoSenderDelegate;
    
    class VideoSenderParams : public NdnParams {
    public:
        // public methods go here
        std::string getConferencePrefix() { return "/ndn/ucla.edu/ndnrtc/test"; };
    };

    class NdnVideoSender : public NdnRtcObject, public IEncodedFrameConsumer {
    public:
        // construction/desctruction
        NdnVideoSender(){};
        ~NdnVideoSender(){};
        
        // public methods go here
        void registerConference(VideoSenderParams &params);
        void fetchParticipantsList();
        
    private:        
        // private attributes go here
        VideoSenderParams params_;
        
        // private methods go here        
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
        // public methods go here
        virtual void onParticipantsListFetched(std::vector<std::string> participantsList) = 0;
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
