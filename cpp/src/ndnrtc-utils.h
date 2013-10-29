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
        
        static unsigned int setupFrequencyMeter();
        static void frequencyMeterTick(unsigned int meterId);
        static double currentFrequencyMeterValue(unsigned int meterId);
        static void releaseFrequencyMeter(unsigned int meterId);
        
        static int frameNumber(const Name::Component &segmentComponent);        
        static int segmentNumber(const Name::Component &segmentComponent);
        
        static webrtc::VoiceEngine *sharedVoiceEngine();
        static void releaseVoiceEngine();
    };
}

#endif /* defined(__ndnrtc__ndnrtc_utils__) */
