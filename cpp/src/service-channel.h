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
        public:
            virtual void
            onProducerParametersUpdated(const ParamsStruct& newVideoParams,
                                        const ParamsStruct& newAudioParams) = 0;
            
            virtual void
            onUpdateFailedWithTimeout() = 0;
            
            virtual void
            onUpdateFailedWithError(const char* errMsg) = 0;
        };
        
        /**
         * Service channel can be used for fetching any non-media information
         * from network or directly from producer
         */
        class ServiceChannel : public ndnlog::new_api::ILoggingObject
        {
        public:
            static unsigned int DefaultUpdateIntervalMs;
            
            ServiceChannel(IServiceChannelCallback* callback,
                           boost::shared_ptr<InterestQueue> interestQueue,
                           unsigned int updateIntervalMs = DefaultUpdateIntervalMs);
            virtual ~ServiceChannel(){}
            
            void
            startMonitor(const std::string& sessionInfoPrefix);
            
            void
            stopMonitor();
            
        private:
            bool isMonitoring_;
            unsigned int updateCounter_, updateIntervalMs_;
            IServiceChannelCallback *callback_;
            boost::shared_ptr<InterestQueue> interestQueue_;
            
            ParamsStruct videoParams_, audioParams_;
            Name sessionInfoPrefix_;
            
            webrtc::ThreadWrapper &monitoringThread_;
            webrtc::EventWrapper &monitorTimer_;
            
            static bool
            processMonitoring(void *obj)
            { return ((ServiceChannel*)obj)->monitor(); }
            
            void
            startMonitorThread();
            
            void
            stopMonitorThread();
            
            bool
            monitor();
            
            void
            requestSessionInfo();
            
            void
            onData(const boost::shared_ptr<const Interest>& interest,
                   const boost::shared_ptr<Data>& data);
            void
            onTimeout(const boost::shared_ptr<const Interest>& interest);
            
            void
            updateParametersFromInfo(const SessionInfo& sessionInfo);
            
            bool
            hasUpdates(const ParamsStruct& oldParams,
                       const ParamsStruct& newParams);
        };
    };
};

#endif /* defined(__ndnrtc__service_channel__) */
