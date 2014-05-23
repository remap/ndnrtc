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

namespace ndnrtc {
    class IRawFrameConsumer;
    
    class CameraCapturer : public NdnRtcObject,
    public webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer(const ParamsStruct &params);
        ~CameraCapturer();

        // public methods go here
        void setFrameConsumer(IRawFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; }
        bool isCapturing() { return (vcm_)?vcm_->CaptureStarted():false; }

        int init();
        int startCapture();
        int stopCapture();
        int numberOfCaptureDevices();
        std::vector<std::string>* availableCaptureDevices();
        void printCapturingInfo();

        // interface conformance - webrtc::VideoCaptureDataCallback
        void OnIncomingCapturedFrame(const int32_t id,
                                     webrtc::I420VideoFrame& videoFrame);
        void OnCaptureDelayChanged(const int32_t id,
                                   const int32_t delay);

        // statistics
        double getCapturingFrequency() { return NdnRtcUtils::currentFrequencyMeterValue(meterId_); }
        
        
    private:        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> capture_cs_;
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &captureThread_;
        webrtc::EventWrapper &captureEvent_;
        webrtc::I420VideoFrame capturedFrame_, deliverFrame_;
        double capturedTimeStamp_ = 0;
        
        // private attributes go here
        webrtc::VideoCaptureCapability capability_;
        webrtc::VideoCaptureModule* vcm_ = nullptr;
        IRawFrameConsumer *frameConsumer_ = nullptr;
        
        // statistics
        unsigned int meterId_;
        
        static bool deliverCapturedFrame(void *obj) { return ((CameraCapturer*)obj)->process(); }

        bool process();
    };
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(webrtc::I420VideoFrame &frame,
                                    double unixTimeStamp) = 0;
    };
}

#endif /* defined(__ndnrtc__camera_capturer__) */
