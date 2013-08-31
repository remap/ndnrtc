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

namespace ndnrtc {
    class IRawFrameConsumer;
    
    class CameraCapturerParams : public NdnParams
    {
    public:
        // construction/desctruction
        CameraCapturerParams():NdnParams(){};
        
        // parameters names
        static const std::string ParamNameDeviceId;
        static const std::string ParamNameWidth;
        static const std::string ParamNameHeight;
        static const std::string ParamNameFPS;
        
        // static public
        static CameraCapturerParams *defaultParams()
        {
            CameraCapturerParams *p = new CameraCapturerParams();
            
            p->setIntParam(CameraCapturerParams::ParamNameDeviceId, 0);
            p->setIntParam(CameraCapturerParams::ParamNameWidth, 640);
            p->setIntParam(CameraCapturerParams::ParamNameHeight, 480);
            p->setIntParam(CameraCapturerParams::ParamNameFPS, 30);
            
            return p;
        };
        
        // public methods
        int getDeviceId(int *did) const { return getParamAsInt(ParamNameDeviceId, did); };
        int getWidth(int *width) const { return getParamAsInt(ParamNameWidth, width); };
        int getHeight(int *height) const { return getParamAsInt(ParamNameHeight, height); };
        int getFPS(int *fps) const { return getParamAsInt(ParamNameFPS, fps); };
    };
    
    class CameraCapturer : public NdnRtcObject, public webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer(const NdnParams *params);
        ~CameraCapturer();
        
        // public methods go here
        void setFrameConsumer(IRawFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; }
        bool isCapturing() { return (vcm_)?vcm_->CaptureStarted():false; }         
        int init();
        int startCapture();
        int stopCapture();
        int numberOfCaptureDevices();
        vector<std::string>* availableCaptureDevices();
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
        IRawFrameConsumer *frameConsumer_ = nullptr;
        
        // private methods
        const CameraCapturerParams *getParams() const { return static_cast<const CameraCapturerParams*>(params_); };
    };
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(webrtc::I420VideoFrame &frame) = 0;
    };
}

#endif /* defined(__ndnrtc__camera_capturer__) */
