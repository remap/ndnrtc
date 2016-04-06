// 
//  ndnrtc-manager.h
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc_manager_h__
#define __ndnrtc_manager_h__

#include "ndnrtc-library.h"

namespace ndnrtc {
    class NdnRtcManager : public INdnRtcLibrary
    {
    public:
        static NdnRtcManager& getSharedInstance();
        
        void
        setObserver(INdnRtcLibraryObserver *observer);
        
        std::string
        startSession(const std::string& username,
                     const new_api::GeneralParams& generalParams,
                     ISessionObserver *sessionObserver);

        std::string
        startSession(const std::string& username,
                     const std::string& prefix,
                     const new_api::GeneralParams& generalParams,
                     ISessionObserver *sessionObserver);
  
        int
        stopSession(const std::string& sessionPrefix);
        
        std::string
        addLocalStream(const std::string& sessionPrefix,
                       const new_api::MediaStreamParams& params,
                       IExternalCapturer** const capturer);

        int
        removeLocalStream(const std::string& sessionPrefix,
                          const std::string& streamPrefix);
        

        std::string
        addRemoteStream(const std::string& remoteSessionPrefix,
                        const std::string& threadName,
                        const new_api::MediaStreamParams& params,
                        const new_api::GeneralParams& generalParams,
                        const new_api::GeneralConsumerParams& consumerParams,
                        IExternalRenderer* const renderer);
        
        std::string
        addRemoteStream(const std::string& remoteSessionPrefix,
                        const std::string& threadName,
                        const new_api::MediaStreamParams& params,
                        const new_api::GeneralParams& generalParams,
                        const new_api::GeneralConsumerParams& consumerParams,
                        ndn::KeyChain* keyChain,
                        IExternalRenderer* const renderer);

        std::string
        removeRemoteStream(const std::string& streamPrefix);
        
        int
        setStreamObserver(const std::string& streamPrefix,
                          IConsumerObserver* const observer);

        int
        removeStreamObserver(const std::string& streamPrefix);
        
        std::string
        getStreamThread(const std::string& streamPrefix);
        
        int
        switchThread(const std::string& streamPrefix,
                     const std::string& threadName);

        new_api::statistics::StatisticsStorage
        getRemoteStreamStatistics(const std::string& streamPrefix);

        void getVersionString(char **versionString);

        void serializeSessionInfo(const new_api::SessionInfo &sessionInfo,
                                          unsigned int &length,
                                          unsigned char **bytes);

        bool deserializeSessionInfo(const unsigned int length,
                                            const unsigned char *bytes,
                                            new_api::SessionInfo &sessionInfo);

        void fatalException(const std::exception& e);
        
        ~NdnRtcManager();
    private:
        bool initialized_, failed_;
        
        NdnRtcManager();
        NdnRtcManager(NdnRtcManager const&) = delete;
        void operator=(NdnRtcManager const&) = delete;
        
        int notifyObserverWithError(const char *format, ...) const;
        int notifyObserverWithState(const char *stateName,
                                    const char *format, ...) const;
        void notifyObserver(const char *state, const char *args) const;
    };
}

#endif