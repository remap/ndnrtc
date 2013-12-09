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

#include "ndnrtc-common.h"
#include "webrtc.h"

#define STR(exp) (#exp)

namespace ndnrtc{
    class NdnRtcUtils {
    public:
        static unsigned int getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize);
        
        static double timestamp() {
            return time(NULL) * 1000.0;
        }
        
        static int64_t millisecondTimestamp();
        static int64_t microsecondTimestamp();
        static int64_t nanosecondTimestamp();
        
        static unsigned int setupFrequencyMeter(unsigned int granularity = 1);
        static void frequencyMeterTick(unsigned int meterId);
        static double currentFrequencyMeterValue(unsigned int meterId);
        static void releaseFrequencyMeter(unsigned int meterId);

        static unsigned int setupDataRateMeter(unsigned int granularity = 1);
        static void dataRateMeterMoreData(unsigned int meterId,
                                          unsigned int dataSize);
        static double currentDataRateMeterValue(unsigned int meterId);
        static void releaseDataRateMeter(unsigned int meterId);
        
        static int frameNumber(const Name::Component &segmentComponent);        
        static int segmentNumber(const Name::Component &segmentComponent);
        
        static Name::Component componentFromInt(unsigned int number);
        
        static webrtc::VoiceEngine *sharedVoiceEngine();
        static void releaseVoiceEngine();
        
        static std::string stringFromFrameType(webrtc::VideoFrameType &frameType);
        
        static unsigned int toFrames(unsigned int intervalMs,
                                     double fps);
        static unsigned int toTimeMs(unsigned int frames,
                                     double fps);
    };
}

#endif /* defined(__ndnrtc__ndnrtc_utils__) */
