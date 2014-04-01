//
//  face-wrapper.h
//  ndnrtc
//
//  Created by Peter Gusev on 2/11/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__face_wrapper__
#define __ndnrtc__face_wrapper__

#include "ndnrtc-common.h"
#include "params.h"

namespace ndnrtc {
    using namespace ndn;
    using namespace ptr_lib;
    
    /**
     * Thread-safe wrapper for NDN face class
     */
    class FaceWrapper : public ndnlog::new_api::ILoggingObject {
    public:
        FaceWrapper();
        FaceWrapper(shared_ptr<Face> &face_);
        ~FaceWrapper();
        
        void setFace(shared_ptr<Face> face) { face_ = face; }
        
        uint64_t
        expressInterest(const Interest& interest,
                        const OnData& onData,
                        const OnTimeout& onTimeout = OnTimeout(),
                        WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        uint64_t registerPrefix(const Name& prefix,
                                const OnInterest& onInterest,
                                const OnRegisterFailed& onRegisterFailed,
                                const ForwardingFlags& flags = ForwardingFlags(),
                                WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        void processEvents();
        void shutdown();
        
    private:
        shared_ptr<Face> face_;
        webrtc::CriticalSectionWrapper &faceCs_;
    };
    
    class FaceProcessor : public ndnlog::new_api::ILoggingObject
    {
    public:
        FaceProcessor(const shared_ptr<FaceWrapper>& faceWrapper);
        ~FaceProcessor();
        
        int
        startProcessing(unsigned int usecInterval = 100);
        
        void
        stopProcessing();
        
        void
        setProcessingInterval(unsigned int usecInterval)
        { usecInterval_ = usecInterval; }
        
        shared_ptr<FaceWrapper>
        getFace()
        { return faceWrapper_; }
        
        shared_ptr<Transport>
        getTransport()
        { return transport_; }
        
        void
        setTransport(shared_ptr<Transport>& transport)
        { transport_ = transport; }
        
        static int
        setupFaceAndTransport(const ParamsStruct &params,
                              shared_ptr<FaceWrapper>& face,
                              shared_ptr<Transport>& transport);
        
        static shared_ptr<FaceProcessor>
        createFaceProcessor(const ParamsStruct& params);
                
    private:
        bool isProcessing_;
        unsigned int usecInterval_;
        shared_ptr<FaceWrapper> faceWrapper_;
        shared_ptr<Transport> transport_;
        webrtc::ThreadWrapper &processingThread_;
        
        static bool
        processFaceEventsRoutine(void *processor)
        {
            return ((FaceProcessor*)processor)->processEvents();
        }
        
        bool
        processEvents();
    };
};

#endif /* defined(__ndnrtc__face_wrapper__) */
