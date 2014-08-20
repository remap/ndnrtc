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

namespace ndnrtc {
    class SessionInfo;
    
    namespace new_api {
        class InterestQueue;
        
        class IServiceChannelCallback
        {
        };
        
        class IServiceChannelListenerCallback : public IServiceChannelCallback
        {
        public:
            virtual void
            onProducerParametersUpdated(const ParamsStruct& newVideoParams,
                                        const ParamsStruct& newAudioParams) = 0;
            
            virtual void
            onUpdateFailedWithTimeout() = 0;
            
            virtual void
            onUpdateFailedWithError(const char* errMsg) = 0;
        };
        
        class IServiceChannelPublisherCallback : public IServiceChannelCallback
        {
        public:
            virtual void
            onSessionInfoBroadcastFailed() = 0;
            
            virtual boost::shared_ptr<SessionInfo>
            onPublishSessionInfo() = 0;
        };
        
        /**
         * Service channel can be used for fetching any non-media information
         * from network or directly from producer
         */
        class ServiceChannel : public ndnlog::new_api::ILoggingObject
        {
        public:
            static unsigned int DefaultUpdateIntervalMs;
            
            ServiceChannel(IServiceChannelPublisherCallback* callback,
                           boost::shared_ptr<FaceProcessor> faceProcessor,
                           unsigned int freshnessIntervalMs = DefaultUpdateIntervalMs);
            
            ServiceChannel(IServiceChannelListenerCallback* callback,
                           boost::shared_ptr<FaceProcessor> faceProcessor,
                           unsigned int updateIntervalMs = DefaultUpdateIntervalMs);
            virtual ~ServiceChannel(){}
            
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
            void
            startSessionInfoBroadcast(const std::string& sessionInfoPrefix,
                                      const boost::shared_ptr<KeyChain> keyChain,
                                      const Name& signingCertificateName);
            
            /**
             * Starts monitoring changes in producer's session
             */
            void
            startMonitor(const std::string& sessionInfoPrefix);
            
            /**
             * Stops monitoring changes in producer's session
             */
            void
            stopMonitor();
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
        private:
            bool isMonitoring_;
            unsigned int updateCounter_, updateIntervalMs_,
            sessionInfoFreshnessMs_ = 1000;
            IServiceChannelCallback *callback_;
            
            Name signingCertificateName_;
            boost::shared_ptr<KeyChain> ndnKeyChain_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
            boost::shared_ptr<InterestQueue> interestQueue_;
            
            ParamsStruct videoParams_, audioParams_;
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
            
            void
            updateParametersFromInfo(const SessionInfo& sessionInfo);
            
            bool
            hasUpdates(const ParamsStruct& oldParams,
                       const ParamsStruct& newParams);
            
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
