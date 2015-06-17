//
//  face-wrapper.h
//  ndnrtc
//
//  Created by Peter Gusev on 2/11/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__face_wrapper__
#define __ndnrtc__face_wrapper__

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "ndnrtc-common.h"
#include "params.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
    using namespace ndn;
    
    /**
     * Thread-safe wrapper for NDN face class
     */
    class FaceWrapper : public ndnlog::new_api::ILoggingObject {
    public:
        FaceWrapper(){}
        FaceWrapper(boost::shared_ptr<Face> &face_);
        ~FaceWrapper(){}
        
        void
        setFace(boost::shared_ptr<Face> face) { face_ = face; }
        boost::shared_ptr<Face>
        getFace() { return face_; }
        
        uint64_t
        expressInterest(const Interest& interest,
                        const OnData& onData,
                        const OnTimeout& onTimeout = OnTimeout(),
                        WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        void
        removePendingInterest(uint64_t interestId);
        
        uint64_t
        registerPrefix(const Name& prefix,
                       const OnInterest& onInterest,
                       const OnRegisterFailed& onRegisterFailed,
                       const ForwardingFlags& flags = ForwardingFlags(),
                       WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        void
        unregisterPrefix(uint64_t prefixId);
        
        void
        setCommandSigningInfo(KeyChain& keyChain, const Name& certificateName);
        void
        processEvents();
        void
        shutdown();
        
        /**
         * Synchronizes with the face's critical section. Can be used in 
         * situations when API requires calls from one thread (e.g. adding to 
         * memory cache and calling processEvents should be on one thread)
         */
        void
        synchronizeStart() { faceMutex_.lock(); }
        void
        synchronizeStop() { faceMutex_.unlock(); }
        
    private:
        boost::shared_ptr<Face> face_;
        boost::mutex faceMutex_;
    };
    
    class FaceProcessor :   public new_api::NdnRtcComponent
    {
    public:
        FaceProcessor(const boost::shared_ptr<FaceWrapper>& faceWrapper);
        ~FaceProcessor();
        
        int
        startProcessing(unsigned int usecInterval = 10000);
        
        void
        stopProcessing();
        
        void
        setProcessingInterval(unsigned int usecInterval)
        { usecInterval_ = usecInterval; }
        
        boost::shared_ptr<FaceWrapper>
        getFaceWrapper()
        { return faceWrapper_; }
        
        boost::shared_ptr<Transport>
        getTransport()
        { return transport_; }
        
        void
        setTransport(boost::shared_ptr<Transport>& transport)
        { transport_ = transport; }
        
        static int
        setupFaceAndTransport(const std::string host, const int port,
                              boost::shared_ptr<FaceWrapper>& face,
                              boost::shared_ptr<Transport>& transport);
        
        static boost::shared_ptr<FaceProcessor>
        createFaceProcessor(const std::string host, const int port,
                            const boost::shared_ptr<ndn::KeyChain>& keyChain = boost::shared_ptr<ndn::KeyChain>(),
                            const boost::shared_ptr<Name>& certificateName = boost::shared_ptr<Name>());
        
    private:
        bool isProcessing_;
        unsigned int usecInterval_;
        boost::shared_ptr<FaceWrapper> faceWrapper_;
        boost::shared_ptr<Transport> transport_;
        boost::thread processEventsThread_;
    };
};

#endif /* defined(__ndnrtc__face_wrapper__) */
