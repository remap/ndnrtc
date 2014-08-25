//
//  camera-capturer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/16/13
//

#ifndef __ndnrtc__camera_capturer__
#define __ndnrtc__camera_capturer__

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"
#include "base-capturer.h"

namespace ndnrtc {
    
    class CameraCapturer : public BaseCapturer,
    public webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer(const ParamsStruct &params);
        ~CameraCapturer();

        int init();
        int startCapture();
        int stopCapture();
        int numberOfCaptureDevices();
        std::vector<std::string>* getAvailableCaptureDevices();
        void printCapturingInfo();

        // interface conformance - webrtc::VideoCaptureDataCallback
        void OnIncomingCapturedFrame(const int32_t id,
                                     webrtc::I420VideoFrame& videoFrame);
        void OnCaptureDelayChanged(const int32_t id,
                                   const int32_t delay);
        
    private:        
        
        // private attributes go here
        webrtc::VideoCaptureCapability capability_;
        webrtc::VideoCaptureModule* vcm_ = nullptr;
    };
    

}

#endif /* defined(__ndnrtc__camera_capturer__) */
