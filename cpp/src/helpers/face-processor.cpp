//
//  face-processor.cpp
//  NDNCon
//
//  Created by Peter Gusev on 2/24/17.
//  Copyright © 2017 UCLA. All rights reserved.
//

#include "helpers/face-processor.hpp"

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>

#define USE_THREADSAFE_FACE

using namespace ndn;
using namespace boost;
using namespace boost::asio;
using namespace ndnrtc::helpers;

namespace ndnrtc {
    namespace helpers {
        class FaceProcessorImpl : public enable_shared_from_this<FaceProcessorImpl>{
        public:
            FaceProcessorImpl(std::string host);
            ~FaceProcessorImpl();

            void start();
            void stop();
            bool isProcessing();

            // non blocking
            void dispatchSynchronized(boost::function<void(boost::shared_ptr<ndn::Face>)> dispatchBlock);
            // blocking
            void performSynchronized(boost::function<void(boost::shared_ptr<ndn::Face>)> dispatchBlock);

            bool initFace();
            void runFace();

            io_service& getIo() { return io_; }
            shared_ptr<Face> getFace() { return face_; }

        private:
            std::string host_;
            shared_ptr<Face> face_;
            thread t_;
            bool isRunningFace_;
            io_service io_;
#ifdef USE_THREADSAFE_FACE
            shared_ptr<io_service::work> ioWork_;
#endif
        };
    }
}

shared_ptr<FaceProcessor>
FaceProcessor::forLocalhost()
{
    return make_shared<FaceProcessor>("localhost");
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
    shared_ptr<MemoryIdentityStorage> identityStorage = make_shared<MemoryIdentityStorage>();
    shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage = make_shared<MemoryPrivateKeyStorage>();
    
    KeyChain keyChain(make_shared<IdentityManager>(identityStorage, privateKeyStorage));
    keyChain.createIdentityAndCertificate("connectivity-check");
    keyChain.getIdentityManager()->setDefaultIdentity("connectivity-check");
#endif
    Face face;
    
    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());
    face.registerPrefix(ndn::Name("/nfd-connectivity-check"),
                        [](const boost::shared_ptr<const ndn::Name>& prefix,
                                                    const boost::shared_ptr<const ndn::Interest>& interest, ndn::Face& face,
                                                    uint64_t interestFilterId,
                                                    const boost::shared_ptr<const ndn::InterestFilter>& filter){},
                        [&done](const boost::shared_ptr<const ndn::Name>& prefix){
                            // failure
                            done = true;
                        },
                        [&done,&registeredPrefixId](const boost::shared_ptr<const ndn::Name>& prefix,
                                                    uint64_t prefId){
                            // success
                            registeredPrefixId = prefId;
                            done = true;
                        });
    while(!done) face.processEvents();
    if (registeredPrefixId) face.removeRegisteredPrefix(registeredPrefixId);
    
    return (registeredPrefixId != 0);
}

FaceProcessor::FaceProcessor(std::string host):
_pimpl(boost::make_shared<FaceProcessorImpl>(host))
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
void FaceProcessor::dispatchSynchronized(function<void (shared_ptr<Face>)> dispatchBlock)
{
    return _pimpl->dispatchSynchronized(dispatchBlock);
}
void FaceProcessor::performSynchronized(function<void (shared_ptr<Face>)> dispatchBlock)
{
    return _pimpl->performSynchronized(dispatchBlock);
}
boost::asio::io_service& FaceProcessor::getIo() { return _pimpl->getIo(); }
boost::shared_ptr<Face> FaceProcessor::getFace() { return _pimpl->getFace(); }
void FaceProcessor::registerPrefix(const Name& prefix, 
                                   const OnInterestCallback& onInterest,
                                   const OnRegisterFailed& onRegisterFailed,
                                   const OnRegisterSuccess& onRegisterSuccess)
{
    _pimpl->performSynchronized([prefix, onInterest, onRegisterFailed, onRegisterSuccess](boost::shared_ptr<ndn::Face> face){
        face->registerPrefix(prefix, onInterest, onRegisterFailed, onRegisterSuccess);
    });
}

void FaceProcessor::registerPrefixBlocking(const ndn::Name& prefix, 
                const OnInterestCallback& onInterest,
                const OnRegisterFailed& onRegisterFailed,
                const OnRegisterSuccess& onRegisterSuccess)
{
    boost::mutex m;
    boost::unique_lock<boost::mutex> lock(m);
    boost::condition_variable isDone;
    boost::atomic<bool> completed(false);
    bool registered = false;

    _pimpl->dispatchSynchronized([prefix, onInterest, onRegisterFailed, onRegisterSuccess](boost::shared_ptr<ndn::Face> face){
        face->registerPrefix(prefix, onInterest, onRegisterFailed, onRegisterSuccess);
    });

    isDone.wait(lock, [&completed](){ return completed.load(); });
}

//******************************************************************************
FaceProcessorImpl::FaceProcessorImpl(std::string host):host_(host), isRunningFace_(false)
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
        ioWork_.reset();
        io_.stop();
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

void FaceProcessorImpl::dispatchSynchronized(boost::function<void (boost::shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        shared_ptr<Face> f = face_;
        io_.dispatch([dispatchBlock, f](){
            dispatchBlock(f);
        });
    }
}

void FaceProcessorImpl::performSynchronized(boost::function<void (boost::shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        if (this_thread::get_id() == t_.get_id())
            dispatchBlock(face_);
        else
        {
            mutex m;
            unique_lock<mutex> lock(m);
            condition_variable isDone;
            atomic<bool> doneFlag(false);
            shared_ptr<Face> face = face_;
            
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
            face_ = make_shared<ThreadsafeFace>(io_);
#else
            face_ = make_shared<Face>();
#endif
        else
#ifdef USE_THREADSAFE_FACE
            face_ = make_shared<ThreadsafeFace>(io_, host_.c_str());
#else
            face_ = make_shared<Face>(host_.c_str());
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
    ioWork_ = make_shared<io_service::work>(io_);
#endif
    isRunningFace_ = false;
    
    shared_ptr<FaceProcessorImpl> self = shared_from_this();
    
    t_ = thread([self](){
        self->isRunningFace_ = true;
        while (self->isRunningFace_)
        {
            try {
#ifdef USE_THREADSAFE_FACE
                self->io_.run();
                self->isRunningFace_ = false;
#else
                self->io_.poll_one();
                self->io_.reset();
                self->face_->processEvents();
#endif
            }
            catch (std::exception &e) {
                // notify about error and try to recover
                if (!self->initFace())
                    self->isRunningFace_ = false;
            }
        }
    });
    
    while (!isRunningFace_) usleep(10000);
}
