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

namespace ndnrtc {
    /**
     * Thread-safe wrapper for NDN face class
     */
    class FaceWrapper : public LoggerObject {
    public:
        FaceWrapper();
        FaceWrapper(shared_ptr<Face> &face_);
        ~FaceWrapper();
        
        void setFace(shared_ptr<Face> face) { face_ = face; }
        
        uint64_t expressInterest(const Interest& interest, const OnData& onData,
                                 const OnTimeout& onTimeout = OnTimeout(),
                                 WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        uint64_t registerPrefix(const Name& prefix, const OnInterest& onInterest,
                                const OnRegisterFailed& onRegisterFailed,
                                const ForwardingFlags& flags = ForwardingFlags(),
                                WireFormat& wireFormat = *WireFormat::getDefaultWireFormat());
        
        void processEvents();
        void shutdown();
        
    private:
        shared_ptr<Face> face_;
        webrtc::CriticalSectionWrapper &faceCs_;
    };
};

#endif /* defined(__ndnrtc__face_wrapper__) */
