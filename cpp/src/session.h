//
//  session.h
//  libndnrtc
//
//  Created by Peter Gusev on 9/18/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#ifndef __libndnrtc__session__
#define __libndnrtc__session__

#include "ndnrtc-common.h"
#include "interfaces.h"
#include "params.h"
#include "ndnrtc-object.h"
#include "service-channel.h"
#include "media-stream.h"
#include "face-wrapper.h"

namespace ndnrtc
{
    namespace new_api
    {
        
        class Session : public NdnRtcComponent,
                        public IServiceChannelPublisherCallback
        {
        public:
            Session(const std::string username,
                    const GeneralParams& generalParams);
            ~Session();
            
            int
            init();
            
            int
            start();
            
            int
            stop();
            
            std::string
            getUserPrefix()
            { return userPrefix_; }
            
        private:
            std::string username_;
            std::string userPrefix_;
            GeneralParams generalParams_;
            
            boost::shared_ptr<KeyChain> userKeyChain_;
            boost::shared_ptr<FaceProcessor> mainFaceProcessor_;
            boost::shared_ptr<new_api::ServiceChannel> serviceChannel_;
           
            typedef std::map<std::string, boost::shared_ptr<MediaStream>> StreamMap;
            StreamMap audioStreams_;
            StreamMap videoStreams_;
            
            void
            startServiceChannel();
            
            // IServiceChannelPublisherCallback
            void
            onSessionInfoBroadcastFailed();
            
            boost::shared_ptr<SessionInfo>
            onPublishSessionInfo();
        };
    }
}

#endif /* defined(__libndnrtc__session__) */
