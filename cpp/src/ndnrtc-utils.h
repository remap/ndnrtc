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

#include "ndnrtc-common.h"
#include "webrtc.h"
#include "params.h"
#include "face-wrapper.h"

#define STR(exp) (#exp)

namespace ndnrtc
{
    using namespace ndn;
    
    extern std::string LIB_LOG;
    
    class NdnRtcUtils {
    public:
        static unsigned int getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize);
        
        static double timestamp() {
            return time(NULL) * 1000.0;
        }
        
        static int64_t millisecondTimestamp();
        static int64_t microsecondTimestamp();
        static int64_t nanosecondTimestamp();
        static double unixTimestamp();
        
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
        
        static unsigned int setupFrequencyMeter(unsigned int granularity = 1);
        static void frequencyMeterTick(unsigned int meterId);
        static double currentFrequencyMeterValue(unsigned int meterId);
        static void releaseFrequencyMeter(unsigned int meterId);

        static unsigned int setupDataRateMeter(unsigned int granularity = 1);
        static void dataRateMeterMoreData(unsigned int meterId,
                                          unsigned int dataSize);
        static double currentDataRateMeterValue(unsigned int meterId);
        static void releaseDataRateMeter(unsigned int meterId);
        
        static unsigned int setupMeanEstimator(unsigned int sampleSize = 0,
                                               double startValue = 0.);
        static void meanEstimatorNewValue(unsigned int estimatorId, double value);
        static double currentMeanEstimation(unsigned int estimatorId);
        static double currentDeviationEstimation(unsigned int estimatorId);
        static void releaseMeanEstimator(unsigned int estimatorId);
        static void resetMeanEstimator(unsigned int estimatorId);
        
        static unsigned int setupSlidingAverageEstimator(unsigned int sampleSize = 2);
        static void slidingAverageEstimatorNewValue(unsigned int estimatorId, double value);
        static double currentSlidingAverageValue(unsigned int estimatorId);
        static double currentSlidingDeviationValue(unsigned int estimatorId);
        static void releaseAverageEstimator(unsigned int estimatorID);
        
        static unsigned int setupFilter(double coeff = 1.);
        static void filterNewValue(unsigned int filterId, double value);
        static double currentFilteredValue(unsigned int filterId);
        static void releaseFilter(unsigned int filterId);
        
        static unsigned int setupInclineEstimator(unsigned int sampleSize = 0);
        static void inclineEstimatorNewValue(unsigned int estimatorId, double value);
        static double currentIncline(unsigned int estimatorId);
        static void releaseInclineEstimator(unsigned int estimatorId);
        
        static int frameNumber(const Name::Component &segmentComponent);        
        static int segmentNumber(const Name::Component &segmentComponent);
        
        static int intFromComponent(const Name::Component &comp);
        static Name::Component componentFromInt(unsigned int number);
        
        static webrtc::VoiceEngine *sharedVoiceEngine();
        static void initVoiceEngine();
        static void releaseVoiceEngine();
        static void startVoiceThread();
        static void stopVoiceThread();
        static void dispatchOnVoiceThread(boost::function<void(void)> dispatchBlock,
                                          boost::function<void(void)> onCompletion);
        
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
