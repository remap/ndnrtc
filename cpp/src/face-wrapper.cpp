//
//  face-wrapper.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 2/11/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "face-wrapper.h"
#include "ndnrtc-utils.h"

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
uint64_t FaceWrapper::expressInterest(const Interest &interest,
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

void
FaceWrapper::setCommandSigningInfo(KeyChain& keyChain,
                                   const Name& certificateName)
{
    CriticalSectionScoped scopedCs(&faceCs_);
    
    face_->setCommandSigningInfo(keyChain, certificateName);
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


//******************************************************************************
//******************************************************************************
#pragma mark - static
int
FaceProcessor::setupFaceAndTransport(const ParamsStruct& params,
                                     shared_ptr<ndnrtc::FaceWrapper>& face,
                                     shared_ptr<ndn::Transport>& transport)
{
    int res = RESULT_OK;
    
    try
    {
        string host = string(params.host);
        int port = params.portNum;
        
        shared_ptr<ndn::Transport::ConnectionInfo>
        connInfo(new TcpTransport::ConnectionInfo(host.c_str(), port));
        
        transport.reset(new TcpTransport());
        
        shared_ptr<Face> ndnFace(new Face(transport, connInfo));
        face.reset(new FaceWrapper(ndnFace));
        
    }
    catch (std::exception &e)
    {
        res = RESULT_ERR;
    }
    
    return res;
}

shared_ptr<FaceProcessor>
FaceProcessor::createFaceProcessor(const ParamsStruct& params,
                                   const shared_ptr<ndn::KeyChain>& keyChain,
                                   const shared_ptr<Name>& certificateName)
{
    shared_ptr<FaceWrapper> face(nullptr);
    shared_ptr<ndn::Transport> transport(nullptr);
    shared_ptr<FaceProcessor> fp(nullptr);
    
    if (RESULT_GOOD(FaceProcessor::setupFaceAndTransport(params, face, transport)))
    {
        if (keyChain.get())
        {
            if (certificateName.get())
                face->setCommandSigningInfo(*keyChain, *certificateName);
            else
                face->setCommandSigningInfo(*keyChain, keyChain->getDefaultCertificateName());
            }
            
        fp.reset(new FaceProcessor(face));
        fp->setTransport(transport);
    }
    
    return fp;
}


//******************************************************************************
#pragma mark - construction/destruction
FaceProcessor::FaceProcessor(const shared_ptr<FaceWrapper>& faceWrapper):
isProcessing_(false),
usecInterval_(100),
faceWrapper_(faceWrapper),
processingThread_(*webrtc::ThreadWrapper::CreateThread(FaceProcessor::processFaceEventsRoutine, this))
{
}

FaceProcessor::~FaceProcessor()
{
    stopProcessing();
}

//******************************************************************************
#pragma mark - public
int
FaceProcessor::startProcessing(unsigned int usecInterval)
{
    if (!isProcessing_)
    {
        usecInterval_ = usecInterval;
        isProcessing_ = true;
        
        unsigned int tid;
        processingThread_.Start(tid);
    }
    return RESULT_OK;
}

void
FaceProcessor::stopProcessing()
{
    if (isProcessing_)
    {
        processingThread_.SetNotAlive();
        isProcessing_ = false;
        processingThread_.Stop();
    }
}

//******************************************************************************
#pragma mark - private
bool
FaceProcessor::processEvents()
{
    faceWrapper_->processEvents();
    usleep(usecInterval_);
    
    return isProcessing_;
}

