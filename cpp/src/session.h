//
//  session.h
//  libndnrtc
//
//  Created by Peter Gusev on 9/18/14.
//  Copyright (c) 2014 REMAP. All rights reserved.
//

#ifndef __libndnrtc__session__
#define __libndnrtc__session__

#include <boost/asio/steady_timer.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include "ndnrtc-common.h"
#include "interfaces.h"
#include "params.h"
#include "ndnrtc-object.h"
#include "media-stream.h"
#include "face-wrapper.h"

namespace ndnrtc
{
    namespace new_api
    {
        
        class Session : public NdnRtcComponent
        {
        public:
            Session();
            ~Session();
            
            int
            init(const std::string username,
                 const GeneralParams& generalParams,
                 boost::shared_ptr<FaceProcessor> mainFaceProcessor);
            
            int
            start();
            
            int
            stop();
            
            /**
             * Adds new local stream to the session
             * @param params Stream parameters
             * @param capturer 
             * @param streamPrefix Upon success, assigned stream prefix value
             */
            int
            addLocalStream(const MediaStreamParams& params,
                           IExternalCapturer** const capturer,
                           std::string& streamPrefix);
            
            /**
             * Removes existing local stream
             */
            int
            removeLocalStream(const std::string& streamPrefix);
            
            std::string
            getPrefix()
            { return userPrefix_; }
            
            void
            setSessionObserver(ISessionObserver* sessionObserver)
            { sessionObserver_ = sessionObserver; }
            
        private:
            std::string username_;
            std::string userPrefix_;
            GeneralParams generalParams_;
            SessionStatus status_;
            boost::recursive_mutex observerMutex_;
            ISessionObserver *sessionObserver_;
            
            boost::shared_ptr<KeyChain> userKeyChain_;
            boost::shared_ptr<FaceProcessor> mainFaceProcessor_;
            boost::shared_ptr<MemoryContentCache> sessionCache_;
           
            typedef std::map<std::string, boost::shared_ptr<MediaStream>> StreamMap;
            StreamMap audioStreams_;
            StreamMap videoStreams_;
            
            void
            switchStatus(SessionStatus status);
            
            bool
            updateSessionInfo();
            
            boost::shared_ptr<SessionInfo>
            getSessionInfo();
            
            void
            onError(const char *errorMessage, const int errorCode);
        };
    }
}

#endif /* defined(__libndnrtc__session__) */
