//
//  ndnrtc-utils.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING

#include <stdarg.h>
#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>

#include "ndnrtc-utils.h"
#include "ndnrtc-endian.h"
#include "ndnrtc-namespace.h"
#include "face-wrapper.h"
#include "ndnrtc-exception.h"
#include "ndnrtc-manager.h"
#include "audio-controller.h"

using namespace std;
using namespace ndnrtc;
using namespace webrtc;

using namespace boost::chrono;

//********************************************************************************
#pragma mark - all static
std::string ndnrtc::LIB_LOG = "ndnrtc-startup.log";
static boost::asio::io_service* NdnRtcIoService;
static boost::shared_ptr<FaceProcessor> LibraryFace;

static VoiceEngine *VoiceEngineInstance = NULL;

static boost::thread backgroundThread;
static boost::shared_ptr<boost::asio::io_service::work> backgroundWork;

void initVE();
void resetThread();

//******************************************************************************
void NdnRtcUtils::setIoService(boost::asio::io_service& ioService)
{
    NdnRtcIoService = &ioService;
}

boost::asio::io_service& NdnRtcUtils::getIoService()
{
    return *NdnRtcIoService;
}

void NdnRtcUtils::startBackgroundThread()
{
    if (!NdnRtcIoService)
        return;
    
    if (!backgroundWork.get() &&
        backgroundThread.get_id() == boost::thread::id())
    {
        backgroundWork.reset(new boost::asio::io_service::work(*NdnRtcIoService));
        resetThread();
    }
}

void NdnRtcUtils::stopBackgroundThread()
{
    if (backgroundWork.get())
    {
        backgroundWork.reset();
        (*NdnRtcIoService).stop();
        backgroundThread = boost::thread();

        if (!isBackgroundThread())
            backgroundThread.try_join_for(boost::chrono::milliseconds(500));
    }
}

bool NdnRtcUtils::isBackgroundThread()
{
    return (boost::this_thread::get_id() == backgroundThread.get_id());
}

void NdnRtcUtils::dispatchOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                        boost::function<void(void)> onCompletion)
{
    if (backgroundWork.get())
    {
        (*NdnRtcIoService).dispatch([=]{
            dispatchBlock();
            if (onCompletion)
                onCompletion();
        });
    }
    else
    {
        throw std::runtime_error("this is not supposed to happen. bg thread is dead already");
    }
}

void NdnRtcUtils::performOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                             boost::function<void(void)> onCompletion)
{
    if (backgroundWork.get())
    {
        if (boost::this_thread::get_id() == backgroundThread.get_id())
        {
            dispatchBlock();
            if (onCompletion)
                onCompletion();
        }
        else
        {
            boost::mutex m;
            boost::unique_lock<boost::mutex> lock(m);
            boost::condition_variable isDone;
            
            (*NdnRtcIoService).post([dispatchBlock, onCompletion, &isDone]{
                dispatchBlock();
                if (onCompletion)
                    onCompletion();
                isDone.notify_one();
            });
            
            isDone.wait(lock);
        }
    }
    else
    {
        throw std::runtime_error("this is not supposed to happen. bg thread is dead already");
    }
}

void NdnRtcUtils::createLibFace(const new_api::GeneralParams& generalParams)
{
    if (!LibraryFace.get() ||
        (LibraryFace.get() && LibraryFace->getTransport()->getIsConnected() == false))
    {
        LogInfo(LIB_LOG) << "Creating library Face..." << std::endl;
        
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_, NdnRtcNamespace::defaultKeyChain());
        LibraryFace->startProcessing();
        
        LogInfo(LIB_LOG) << "Library Face created" << std::endl;
    }
}

boost::shared_ptr<FaceProcessor> NdnRtcUtils::getLibFace()
{
    return LibraryFace;
}

void NdnRtcUtils::destroyLibFace()
{
    if (LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Stopping library Face..." << std::endl;
        LibraryFace->stopProcessing();
        LibraryFace.reset();
        LogInfo(LIB_LOG) << "Library face stopped" << std::endl;
    }
}

//******************************************************************************

unsigned int NdnRtcUtils::getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize)
{
    return (unsigned int)ceil((float)dataSize/(float)segmentSize);
}

int NdnRtcUtils::segmentNumber(const Name::Component &segmentNoComponent)
{
    std::vector<unsigned char> bytes = *segmentNoComponent.getValue();
    int bytesLength = segmentNoComponent.getValue().size();
    int result = 0;
    unsigned int i;
    
    for (i = 0; i < bytesLength; ++i) {
        result *= 256.0;
        result += (int)bytes[i];
    }
    
    return result;
}

int NdnRtcUtils::frameNumber(const Name::Component &frameNoComponent)
{
    return NdnRtcUtils::intFromComponent(frameNoComponent);
}

int NdnRtcUtils::intFromComponent(const Name::Component &comp)
{
    std::vector<unsigned char> bytes = *comp.getValue();
    int valueLength = comp.getValue().size();
    int result = 0;
    unsigned int i;
    
    for (i = 0; i < valueLength; ++i) {
        unsigned char digit = bytes[i];
        if (!(digit >= '0' && digit <= '9'))
            return -1;
        
        result *= 10;
        result += (unsigned int)(digit - '0');
    }
    
    return result;    
}

Name::Component NdnRtcUtils::componentFromInt(unsigned int number)
{
    stringstream ss;
    
    ss << number;
    std::string frameNoStr = ss.str();
    
    return Name::Component((const unsigned char*)frameNoStr.c_str(),
                           frameNoStr.size());
}

//******************************************************************************
string NdnRtcUtils::stringFromFrameType(const WebRtcVideoFrameType &frameType)
{
    switch (frameType) {
        case webrtc::kDeltaFrame:
            return "DELTA";
        case webrtc::kKeyFrame:
            return "KEY";
        case webrtc::kAltRefFrame:
            return "ALT-REF";
        case webrtc::kGoldenFrame:
            return "GOLDEN";
        case webrtc::kSkipFrame:
            return "SKIP";
        default:
            return "UNKNOWN";
    }
}

unsigned int NdnRtcUtils::toFrames(unsigned int intervalMs,
                                   double fps)
{
    return (unsigned int)ceil(fps*(double)intervalMs/1000.);
}

unsigned int NdnRtcUtils::toTimeMs(unsigned int frames,
                                   double fps)
{
    return (unsigned int)ceil((double)frames/fps*1000.);
}

uint32_t NdnRtcUtils::generateNonceValue()
{
    uint32_t nonce = (uint32_t)std::rand();
    
    return nonce;
}

Blob NdnRtcUtils::nonceToBlob(const uint32_t nonceValue)
{
    uint32_t beValue = htobe32(nonceValue);
    Blob b((uint8_t *)&beValue, sizeof(uint32_t));
    return b;
}

uint32_t NdnRtcUtils::blobToNonce(const ndn::Blob &blob)
{
    if (blob.size() < sizeof(uint32_t))
        return 0;
    
    uint32_t beValue = *(uint32_t *)blob.buf();
    return be32toh(beValue);
}

std::string NdnRtcUtils::getFullLogPath(const new_api::GeneralParams& generalParams,
                                  const std::string& fileName)
{
    static char logPath[PATH_MAX];
    return ((generalParams.logPath_ == "")?std::string(getwd(logPath)):generalParams.logPath_) + "/" + fileName;
}


std::string NdnRtcUtils::toString(const char *format, ...)
{
    std::string str = "";
    
    if (format)
    {
        char *stringBuf = (char*)malloc(256);
        memset((void*)stringBuf, 0, 256);
        
        va_list args;
        
        va_start(args, format);
        vsprintf(stringBuf, format, args);
        va_end(args);
        
        str = string(stringBuf);
        free(stringBuf);
    }
    
    return str;
}

//******************************************************************************
static bool ThreadRecovery = false;
void resetThread()
{
    backgroundThread = boost::thread([](){
        try
        {
            if (ThreadRecovery)
            {
                ThreadRecovery = false;
            }
            
            NdnRtcIoService->run();
        }
        catch (std::exception &e) // fatal
        {
            NdnRtcIoService->reset();
            NdnRtcManager::getSharedInstance().fatalException(e);
            ThreadRecovery = true;
            resetThread();
        }
    });
}
