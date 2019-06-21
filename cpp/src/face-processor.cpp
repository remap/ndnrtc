//
//  face-processor.cpp
//  NDNCon
//
//  Created by Peter Gusev on 2/24/17.
//  Copyright © 2017 UCLA. All rights reserved.
//

#include "face-processor.hpp"
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>

// #define USE_THREADSAFE_FACE

using namespace ndn;

class FaceProcessorImpl : public std::enable_shared_from_this<FaceProcessorImpl>{
public:
    FaceProcessorImpl(std::string host, bool createThread);
    ~FaceProcessorImpl();

    void start();
    void stop();
    bool isProcessing();

    // non blocking
    void dispatchSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);
    // blocking
    void performSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);

    bool initFace();
    void runFace();

    boost::asio::io_service& getIo() { return io_; }
    std::shared_ptr<Face> getFace() { return face_; }

private:
    bool createThread_;
    std::string host_;
    std::shared_ptr<Face> face_;
    std::thread t_;
    bool isRunningFace_;
    boost::asio::io_service io_;
#ifdef USE_THREADSAFE_FACE
    std::shared_ptr<boost::asio::io_service::work> ioWork_;
#endif

    void processEvents();
};

std::shared_ptr<FaceProcessor>
FaceProcessor::forLocalhost()
{
    return std::make_shared<FaceProcessor>("localhost");
}
bool FaceProcessor::checkNfdConnection()
{
    bool done = false;
    uint64_t registeredPrefixId = 0;
#if 0
    KeyChain keyChain;
#else
    // using in-memory keychain as a workaround to avoid exception
    // trying to sign with the default key
    std::shared_ptr<MemoryIdentityStorage> identityStorage = std::make_shared<MemoryIdentityStorage>();
    std::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage = std::make_shared<MemoryPrivateKeyStorage>();

    KeyChain keyChain(std::make_shared<IdentityManager>(identityStorage, privateKeyStorage));
    keyChain.createIdentityAndCertificate("connectivity-check");
    keyChain.getIdentityManager()->setDefaultIdentity("connectivity-check");
#endif
    Face face;

    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());
    face.registerPrefix(ndn::Name("/nfd-connectivity-check"),
                        [](const std::shared_ptr<const ndn::Name>& prefix,
                                                    const std::shared_ptr<const ndn::Interest>& interest, ndn::Face& face,
                                                    uint64_t interestFilterId,
                                                    const std::shared_ptr<const ndn::InterestFilter>& filter){},
                        [&done](const std::shared_ptr<const ndn::Name>& prefix){
                            // failure
                            done = true;
                        },
                        [&done,&registeredPrefixId](const std::shared_ptr<const ndn::Name>& prefix,
                                                    uint64_t prefId){
                            // success
                            registeredPrefixId = prefId;
                            done = true;
                        });
    while(!done) face.processEvents();
    if (registeredPrefixId) face.removeRegisteredPrefix(registeredPrefixId);

    return (registeredPrefixId != 0);
}

FaceProcessor::FaceProcessor(std::string host, bool createThread):
_pimpl(std::make_shared<FaceProcessorImpl>(host, createThread))
{
    if (_pimpl->initFace())
        _pimpl->runFace();
    else
        throw std::runtime_error("couldn't initialize face object");
}

FaceProcessor::~FaceProcessor() {
    _pimpl->stop();
    _pimpl.reset();
}
void FaceProcessor::start() { _pimpl->start(); }
void FaceProcessor::stop() { _pimpl->stop(); }
bool FaceProcessor::isProcessing() { return _pimpl->isProcessing(); }
void FaceProcessor::processEvents() { _pimpl->runFace(); }
void FaceProcessor::dispatchSynchronized(std::function<void (std::shared_ptr<Face>)> dispatchBlock)
{
    return _pimpl->dispatchSynchronized(dispatchBlock);
}
void FaceProcessor::performSynchronized(std::function<void (std::shared_ptr<Face>)> dispatchBlock)
{
    return _pimpl->performSynchronized(dispatchBlock);
}
boost::asio::io_service& FaceProcessor::getIo() { return _pimpl->getIo(); }
std::shared_ptr<Face> FaceProcessor::getFace() { return _pimpl->getFace(); }

//******************************************************************************
FaceProcessorImpl::FaceProcessorImpl(std::string host, bool createThread)
: host_(host)
#ifdef USE_THREADSAFE_FACE
, createThread_(true)
#else
, createThread_(createThread)
#endif
, isRunningFace_(false)
{
}

FaceProcessorImpl::~FaceProcessorImpl()
{
    stop();
}

void FaceProcessorImpl::start()
{
    if (!isRunningFace_)
        if (initFace())
            runFace();
}

void FaceProcessorImpl::stop()
{
    if (isRunningFace_)
    {
        isRunningFace_ = false;

#ifdef USE_THREADSAFE_FACE
        face_->shutdown();

        std::cout << "work reset" << std::endl;
        ioWork_.reset();

        std::cout << "t join" << std::endl;
        t_.join();
#else
//        std::cout << "t join" << std::endl;
        t_.join();
        io_.stop();
        face_->shutdown();
#endif
//        std::cout << "stopped" << std::endl;
    }
}

bool FaceProcessorImpl::isProcessing()
{
    return isRunningFace_;
}

void FaceProcessorImpl::dispatchSynchronized(std::function<void (std::shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        std::shared_ptr<Face> f = face_;
        io_.dispatch([dispatchBlock, f](){
            dispatchBlock(f);
        });
    }
}

void FaceProcessorImpl::performSynchronized(std::function<void (std::shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        if (std::this_thread::get_id() == t_.get_id() || createThread_ == false)
            dispatchBlock(face_);
        else
        {
            boost::mutex m;
            boost::unique_lock<boost::mutex> lock(m);
            boost::condition_variable isDone;
            boost::atomic<bool> doneFlag(false);
            std::shared_ptr<Face> face = face_;

            io_.dispatch([dispatchBlock, face, &isDone, &doneFlag](){
                dispatchBlock(face);
                doneFlag = true;
                isDone.notify_one();
            });

            isDone.wait(lock, [&doneFlag](){ return doneFlag.load(); });
        }
    }
}

bool FaceProcessorImpl::initFace()
{
    try {
        if (host_ == "localhost")
#ifdef USE_THREADSAFE_FACE
            face_ = std::make_shared<ThreadsafeFace>(io_);
#else
            face_ = std::make_shared<Face>();
#endif
        else
#ifdef USE_THREADSAFE_FACE
            face_ = std::make_shared<ThreadsafeFace>(io_, host_.c_str());
#else
            face_ = std::make_shared<Face>(host_.c_str());
#endif
    }
    catch(std::exception &e)
    {
        // notify about error
        return false;
    }

    return true;
}

void FaceProcessorImpl::runFace()
{
#ifdef USE_THREADSAFE_FACE
    ioWork_ = std::make_shared<boost::asio::io_service::work>(io_);
#endif
    isRunningFace_ = false;

    std::shared_ptr<FaceProcessorImpl> self = shared_from_this();

    if (createThread_)
    {
        t_ = std::thread([self](){
            self->isRunningFace_ = true;
            while (self->isRunningFace_)
            {
                self->processEvents();
            }
        });

        while (!isRunningFace_) usleep(10000);
    }
    else
    {
        isRunningFace_ = true;
        processEvents();
    }
}

void FaceProcessorImpl::processEvents()
{
    try {
#ifdef USE_THREADSAFE_FACE
        io_.run();
        isRunningFace_ = false;
#else
        io_.poll_one();
        io_.reset();
        face_->processEvents();
#endif
    }
    catch (std::exception &e) {
        // notify about error and try to recover
        if (!initFace())
            isRunningFace_ = false;
    }
}
