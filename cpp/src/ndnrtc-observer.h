//
//  ndnrtc-observer.h
//  ndnrtc
//
//  Created by Peter Gusev on 3/21/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef ndnrtc_ndnrtc_observer_h
#define ndnrtc_ndnrtc_observer_h

namespace ndnrtc {
    class INdnRtcObjectObserver {
    public:
        virtual ~INdnRtcObjectObserver(){}
        
        // public methods go here
        virtual void onErrorOccurred(const char *errorMessage) = 0;
    };
}

#endif
