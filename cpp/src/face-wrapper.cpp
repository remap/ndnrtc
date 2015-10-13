//
//  face-wrapper.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 2/11/14.
//  Copyright 2013-2015 Regents of the University of California
//

#define USE_TS_FACE

#include <unistd.h>
#include <ndn-cpp/threadsafe-face.hpp>

#ifdef USE_TS_FACE
#include <ndn-cpp/transport/async-tcp-transport.hpp>
#include <ndn-cpp/transport/async-unix-transport.hpp>
#endif

#include "face-wrapper.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace boost;

//******************************************************************************
#pragma mark - construction/destruction
FaceWrapper::FaceWrapper(shared_ptr<Face> &face):
face_(face)
{
}

//******************************************************************************
#pragma mark - public
uint64_t FaceWrapper::expressInterest(const Interest &interest,
                                      const OnData &onData,
                                      const OnTimeout& onTimeout,
                                      WireFormat& wireFormat)
{
    uint64_t iid = 0;
    
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    iid = face_->expressInterest(interest, onData, onTimeout, wireFormat);
    
    return iid;
}

void
FaceWrapper::removePendingInterest(uint64_t interestId)
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    face_->removePendingInterest(interestId);
}

uint64_t FaceWrapper::registerPrefix(const Name& prefix,
                                     const OnInterest& onInterest,
                                     const OnRegisterFailed& onRegisterFailed,
                                     const ForwardingFlags& flags,
                                     WireFormat& wireFormat)
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    return face_->registerPrefix(prefix, onInterest, onRegisterFailed, flags,
                                 wireFormat);
}

void
FaceWrapper::unregisterPrefix(uint64_t prefixId)
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    face_->removeRegisteredPrefix(prefixId);
}

void
FaceWrapper::setCommandSigningInfo(KeyChain& keyChain,
                                   const Name& certificateName)
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    face_->setCommandSigningInfo(keyChain, certificateName);
}

void FaceWrapper::processEvents()
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    face_->processEvents();
}

void FaceWrapper::shutdown()
{
    lock_guard<recursive_mutex> scopedLock(faceMutex_);
    face_->shutdown();
}

//******************************************************************************
#pragma mark - private


//******************************************************************************
//******************************************************************************
#pragma mark - static
static std::string getUnixSocketFilePathForLocalhost()
{
    std::string filePath = "/var/run/nfd.sock";
    if (access(filePath.c_str(), R_OK) == 0)
        return filePath;
    else {
        filePath = "/tmp/.ndnd.sock";
        if (access(filePath.c_str(), R_OK) == 0)
            return filePath;
        else
            return "";
    }
}

static shared_ptr<ndn::Transport> getDefaultTransport(asio::io_service& ioService)
{
    if (getUnixSocketFilePathForLocalhost() == "")
#ifdef USE_TS_FACE
        return make_shared<AsyncTcpTransport>(ioService);
#else
    return make_shared<TcpTransport>();
#endif
    else
#ifdef USE_TS_FACE
        return make_shared<AsyncUnixTransport>(ioService);
#else
        return make_shared<UnixTransport>();
#endif
}

static shared_ptr<ndn::Transport::ConnectionInfo> getDefaultConnectionInfo()
{
    std::string filePath = getUnixSocketFilePathForLocalhost();
    if (filePath == "")
#ifdef USE_TS_FACE
        return make_shared<AsyncTcpTransport::ConnectionInfo>("localhost");
#else
        return make_shared<TcpTransport::ConnectionInfo>("localhost");
#endif
    else
#ifdef USE_TS_FACE
        return shared_ptr<AsyncUnixTransport::ConnectionInfo>
        (new AsyncUnixTransport::ConnectionInfo(filePath.c_str()));
#else
    return shared_ptr<UnixTransport::ConnectionInfo>
    (new UnixTransport::ConnectionInfo(filePath.c_str()));

#endif
}

int
FaceProcessor::setupFaceAndTransport(const std::string host, const int port,
                                     shared_ptr<ndnrtc::FaceWrapper>& face,
                                     shared_ptr<ndn::Transport>& transport)
{
    int res = RESULT_OK;
    
    try
    {
        shared_ptr<ndn::Transport::ConnectionInfo> connInfo;
        shared_ptr<Face> ndnFace;
        
        if (host == "127.0.0.1" || host == "0.0.0.0" || host == "localhost")
        {
            transport = getDefaultTransport(NdnRtcUtils::getIoService());
#ifdef USE_TS_FACE
            ndnFace.reset(new ThreadsafeFace(NdnRtcUtils::getIoService(), transport, getDefaultConnectionInfo()));
#else
            ndnFace.reset(new Face(transport, getDefaultConnectionInfo()));
#endif
        }
        else
        {
#ifdef USE_TS_FACE
            connInfo.reset(new AsyncTcpTransport::ConnectionInfo(host.c_str(), port));
            transport.reset(new AsyncTcpTransport(NdnRtcUtils::getIoService()));
            ndnFace.reset(new ThreadsafeFace(NdnRtcUtils::getIoService(), transport, connInfo));
#else
            connInfo.reset(new TcpTransport::ConnectionInfo(host.c_str(), port));
            transport.reset(new TcpTransport());
            ndnFace.reset(new Face(transport, connInfo));
#endif
        }
        
        face.reset(new FaceWrapper(ndnFace));
        
    }
    catch (std::exception &e)
    {
        res = RESULT_ERR;
    }
    
    return res;
}

boost::shared_ptr<FaceProcessor>
FaceProcessor::createFaceProcessor(const std::string host, const int port,
                                   const boost::shared_ptr<ndn::KeyChain>& keyChain,
                                   const boost::shared_ptr<Name>& certificateName)
{
    shared_ptr<FaceWrapper> face;
    shared_ptr<ndn::Transport> transport;
    shared_ptr<FaceProcessor> fp;
    
    if (RESULT_GOOD(FaceProcessor::setupFaceAndTransport(host, port, face, transport)))
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
faceWrapper_(faceWrapper)
{
}

FaceProcessor::~FaceProcessor()
{
    stopProcessing();
    faceWrapper_->shutdown();
    transport_.reset();
    
    std::cout << description_ << " face processor dtor" << std::endl;
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
        
#ifndef US_TS_FACE
        scheduleJob(usecInterval, [this]()->bool{
            try
            {
                faceWrapper_->processEvents();
            }
            catch (std::exception &exception)
            {
                // do nothing
            }
            return this->isProcessing_;
        });
#endif
    }
    return RESULT_OK;
}

void
FaceProcessor::stopProcessing()
{
    if (isProcessing_)
    {
        isProcessing_ = false;
#ifndef USE_TS_FACE
        stopJob();
#endif
    }
}
