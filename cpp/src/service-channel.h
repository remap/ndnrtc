//
//  service-channel.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__service_channel__
#define __ndnrtc__service_channel__

#include "ndnrtc-common.h"
#include "face-wrapper.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
    
    namespace new_api {
        class InterestQueue;
        
        typedef IRemoteSessionObserver IServiceChannelListenerCallback;
        
        class IServiceChannelPublisherCallback
        {
        public:
            virtual void
            onSessionInfoBroadcastFailed() = 0;
            
            virtual boost::shared_ptr<new_api::SessionInfo>
            onPublishSessionInfo() = 0;
        };
        
        /**
         * Service channel can be used for fetching any non-media information
         * from network or directly from producer
         */
        class ServiceChannel :  public NdnRtcComponent
        {
        public:
            static unsigned int DefaultUpdateIntervalMs;
            
            ServiceChannel(IServiceChannelPublisherCallback* callback,
                           boost::shared_ptr<FaceProcessor> faceProcessor,
                           unsigned int freshnessIntervalMs = DefaultUpdateIntervalMs);
            
            ServiceChannel(IServiceChannelListenerCallback* callback,
                           boost::shared_ptr<FaceProcessor> faceProcessor,
                           unsigned int updateIntervalMs = DefaultUpdateIntervalMs);
            virtual ~ServiceChannel(){ callback_ = NULL; }
            
            /**
             * Registers a prefix for publishing producer's session info. 
             * NOTE: After this call, producer should be ready to answer 
             * incoming session info interests with prepared SessionInfo 
             * structure
             * @param sessionInfoPrefix Session info prefix which is registered 
             * using provided face processor
             * @param keyChain Key chain which will be used for signing session 
             * info data
             * @param signingCertificateName Certificate name which will be used 
             * for signing session info data
             * @see onPublishSessionInfo call
             */
            int
            startSessionInfoBroadcast(const std::string& sessionInfoPrefix,
                                      const boost::shared_ptr<KeyChain> keyChain,
                                      const Name& signingCertificateName);
            
            int
            stopSessionInfoBroadcast();
            
            /**
             * Starts monitoring changes in producer's session
             */
            void
            startMonitor(const std::string& sessionPrefix);
            
            /**
             * Stops monitoring changes in producer's session
             */
            void
            stopMonitor();
            
        private:
            bool isMonitoring_;
            unsigned int updateCounter_, updateIntervalMs_,
            sessionInfoFreshnessMs_ = 1000;
            uint64_t registeredPrefixId_, pendingInterestId_;
            void *callback_;
            
            Name signingCertificateName_;
            boost::shared_ptr<KeyChain> ndnKeyChain_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
            
            boost::shared_ptr<new_api::SessionInfo> sessionInfo_;
            Name sessionInfoPrefix_;
            
            webrtc::ThreadWrapper &monitoringThread_;
            webrtc::EventWrapper &monitorTimer_;
            
            static bool
            processMonitoring(void *obj)
            { return ((ServiceChannel*)obj)->monitor(); }
            
            // private
            void
            init();
            
            void
            startMonitorThread();
            
            void
            stopMonitorThread();
            
            bool
            monitor();
            
            void
            requestSessionInfo();
            
            IServiceChannelListenerCallback*
            getCallbackAsListener()
            { return (IServiceChannelListenerCallback*)callback_; }
            
            IServiceChannelPublisherCallback*
            getCallbackAsPublisher()
            { return (IServiceChannelPublisherCallback*)callback_; }
            
            // NDN-CPP callbacks
            void
            onData(const boost::shared_ptr<const Interest>& interest,
                   const boost::shared_ptr<Data>& data);
            void
            onTimeout(const boost::shared_ptr<const Interest>& interest);
            
            void
            onInterest(const boost::shared_ptr<const Name>& prefix,
                       const boost::shared_ptr<const Interest>& interest,
                       ndn::Transport& transport);
            
            void
            onRegisterFailed(const boost::shared_ptr<const Name>&
                             prefix);
        };
    };
};

#endif /* defined(__ndnrtc__service_channel__) */
