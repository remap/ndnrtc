//
//  ndnrtc-library.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__ndnrtc_library__
#define __ndnrtc__ndnrtc_library__

#include "params.h"
#include "statistics.h"
#include "interfaces.h"

namespace ndnrtc {
    
    /**
     * This class provides interface to work with NDN-RTC library.
     * It provides calls to allow publish audio/video streams and fetch them.
     * User should create an instance of this class using static methods 
     * provided. All further communications with NDN-RTC library should be 
     * performed using this instance.
     * No upper boundary exists for the number of simultaneously fetched 
     * streams. Library is configured using ParamsStruct structure.
     */
    class NdnRtcLibrary {
    public:
        /**
         * Returns a reference to the singleton instance of NdnRtcLirary class
         */
        static INdnRtcLibrary& getSharedInstance();
        
    private:
        NdnRtcLibrary();
        NdnRtcLibrary(NdnRtcLibrary const&) = delete;
        void operator=(NdnRtcLibrary const&) = delete;
    };
}


#endif /* defined(__ndnrtc__ndnrtc_library__) */
