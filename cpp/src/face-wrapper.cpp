//
//  face-wrapper.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 2/11/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "face-wrapper.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
FaceWrapper::FaceWrapper():
faceCs_(*CriticalSectionWrapper::CreateCriticalSection())
{
}

FaceWrapper::FaceWrapper(shared_ptr<Face> &face):
faceCs_(*CriticalSectionWrapper::CreateCriticalSection()),
face_(face)
{
}

FaceWrapper::~FaceWrapper()
{
}

//******************************************************************************
#pragma mark - public
uint64_t FaceWrapper::expressInterest(const ndn::Interest &interest,
                                      const OnData &onData,
                                      const OnTimeout& onTimeout,
                                      WireFormat& wireFormat)
{
    CriticalSectionScoped scopedCs(&faceCs_);
    
    return face_->expressInterest(interest, onData, onTimeout, wireFormat);
}

uint64_t FaceWrapper::registerPrefix(const Name& prefix,
                                     const OnInterest& onInterest,
                                     const OnRegisterFailed& onRegisterFailed,
                                     const ForwardingFlags& flags,
                                     WireFormat& wireFormat)
{
    CriticalSectionScoped scopedCs(&faceCs_);
    
    return face_->registerPrefix(prefix, onInterest, onRegisterFailed, flags,
                                 wireFormat);
}

void FaceWrapper::processEvents()
{
    CriticalSectionScoped scopedCs(&faceCs_);
    
    face_->processEvents();
}

void FaceWrapper::shutdown()
{
    CriticalSectionScoped scopedCs(&faceCs_);
    
    face_->shutdown();
}

//******************************************************************************
#pragma mark - private

