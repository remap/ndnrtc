//
//  ndnrtc-utils.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__ndnrtc_utils__
#define __ndnrtc__ndnrtc_utils__

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <ndn-cpp/name.hpp>

#include "webrtc.h"

#define STR(exp) (#exp)

namespace ndnrtc
{
    namespace new_api {
        class GeneralParams;
    }
    
    using namespace ndn;
    extern std::string LIB_LOG;
    class FaceProcessor;
    
    class NdnRtcUtils {
    public:
        static unsigned int getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize);
        
        static double timestamp() {
            return time(NULL) * 1000.0;
        }

        static void setIoService(boost::asio::io_service& ioService);
        static boost::asio::io_service& getIoService();
        static void startBackgroundThread();
        static void stopBackgroundThread();
        static bool isBackgroundThread();
        static void dispatchOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                               boost::function<void(void)> onCompletion = boost::function<void(void)>());
        // synchronous version of dispatchOnBackgroundThread
        static void performOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                               boost::function<void(void)> onCompletion = boost::function<void(void)>());
        
        static void createLibFace(const new_api::GeneralParams& params);
        static boost::shared_ptr<FaceProcessor> getLibFace();
        static void destroyLibFace();
        
        static int frameNumber(const Name::Component &segmentComponent);        
        static int segmentNumber(const Name::Component &segmentComponent);
        
        static int intFromComponent(const Name::Component &comp);
        static Name::Component componentFromInt(unsigned int number);
        
        static std::string stringFromFrameType(const WebRtcVideoFrameType &frameType);
        
        static unsigned int toFrames(unsigned int intervalMs,
                                     double fps);
        static unsigned int toTimeMs(unsigned int frames,
                                     double fps);
        
        static uint32_t generateNonceValue();
        static Blob nonceToBlob(const uint32_t nonceValue);
        static uint32_t blobToNonce(const Blob &blob);
        
        static std::string getFullLogPath(const new_api::GeneralParams& generalParams,
                                          const std::string& fileName);
        static std::string toString(const char *format, ...);
    };
}

#endif /* defined(__ndnrtc__ndnrtc_utils__) */
