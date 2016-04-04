// 
// client-session-observer.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __client_session_observer_h__
#define __client_session_observer_h__

#include <stdlib.h>
#include "include/interfaces.h"

class ClientSessionObserver : public ndnrtc::ISessionObserver
{
	public:
        void
        onSessionStatusUpdate(const char* username, const char* sessionPrefix,
                              ndnrtc::SessionStatus status)
        {
        	LogInfo("") << "client session status update: " 
        	<< status << std::endl;
        }

        void
        onSessionError(const char* username, const char* sessionPrefix,
                       ndnrtc::SessionStatus status, unsigned int errorCode,
                       const char* errorMessage)
        {
        	LogError("") << "client session error: " 
        	<< errorCode << " " << errorMessage
        	<< " status " << status << std::endl;
        }
        
        void
        onSessionInfoUpdate(const ndnrtc::new_api::SessionInfo& sessionInfo)
        {
        	LogTrace("") << "client session info update: " 
        	<< sessionInfo << std::endl;
        }

        void setSessionPrefix(std::string p) { sessionPrefix_ = p; }
        std::string getSessionPrefix() { return sessionPrefix_; }
        void setPrefix(std::string p) { prefix_ = p; }
        std::string getPrefix() { return prefix_; }
        void setUsername(std::string u) { username_ = u; }
        std::string getUsername() { return username_; }

    private:
    	std::string sessionPrefix_, username_, prefix_;

};

#endif
