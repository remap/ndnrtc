//
//  camera-capturer.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/16/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__camera_capturer__
#define __ndnrtc__camera_capturer__

#include <webrtc.h>

namespace ndnrtc {
    class CameraCapturerDelegate;
    
    class CameraCapturer : webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer();
        ~CameraCapturer();
        
        // <#public static attributes go here#>
        
        // <#public static methods go here#>
        
        // <#public attributes go here#>
        
        // public methods go here
        void startCatpturing();
        
    private:
        // <#private static attributes go here#>
        
        // <#private static methods go here#>
        
        // <#private attributes go here#>
        
        // <#private methods go here#>

    }

}

#endif /* defined(__ndnrtc__camera_capturer__) */
