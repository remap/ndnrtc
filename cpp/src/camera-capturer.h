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
        
        // parameters names
        static const std::string ParamNameDeviceId;
        static const std::string ParamNameWidth;
        static const std::string ParamNameHeight;
        static const std::string ParamNameFPS;
        
        // public methods
        int getDeviceId(int *did) { return getParamAsInt(ParamNameDeviceId, did); };
        int getWidth(int *width) { return getParamAsInt(ParamNameWidth, width); };
        int getHeight(int *height) { return getParamAsInt(ParamNameHeight, height); };
        int getFPS(int *fps) { return getParamAsInt(ParamNameFPS, fps); };
    };
    
    class CameraCapturer : public NdnRtcObject, public webrtc::VideoCaptureDataCallback
    {
    public:
        // construction/desctruction
        CameraCapturer(CameraCapturerParams *params);
//        CameraCapturer(IRawFrameConsumer *delegate):delegate_(delegate){};
        ~CameraCapturer();
        
        // public static attributes go here
        
        // <#public static methods go here#>
        
        // <#public attributes go here#>
        
        // public methods go here
        void setFrameConsumer(IRawFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; };
        int init();
        int startCapture();
        int stopCapture();
        
        // interface conformance - webrtc::VideoCaptureDataCallback
        void OnIncomingCapturedFrame(const int32_t id,
                                     webrtc::I420VideoFrame& videoFrame);
        void OnCaptureDelayChanged(const int32_t id,
                                   const int32_t delay);
        
    private:
//        class CameraCapturerTask : public nsRunnable
//        {
//        public:
//            CameraCapturerTask(webrtc::VideoCaptureCapability capability, webrtc::VideoCaptureModule *vcm) : capability_(capability), vcm_(vcm){}
//            ~CameraCapturerTask() { TRACE("task desctroyed"); }
//            
//            NS_IMETHOD Run() {
//                vcm_->StartCapture(capability_);
//                
//                if (!vcm_->CaptureStarted())
//                {
//                    ERROR("capture failed to start");
//                    return NS_OK;
//                }
//                
//                INFO("started camera capture");
//                
//                return NS_OK;
//            }
//            
//        private:
//            webrtc::VideoCaptureModule *vcm_;
//            webrtc::VideoCaptureCapability capability_;            
//        };
        
        // private attributes go here
        webrtc::VideoCaptureCapability capability_;
        webrtc::VideoCaptureModule *vcm_;
        IRawFrameConsumer *frameConsumer_;
//        nsCOMPtr<nsIThread> capturingThread_;
        
        
        // private methods go here
        CameraCapturerParams *getParams() { return static_cast<CameraCapturerParams*>(params_); };
//        void startBackgroundCapturingThread();
//        void stopBackgroundCapturingThread() {
//            if (capturingThread_)
//                capturingThread_->Shutdown();
//                };
    };
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(webrtc::I420VideoFrame &frame) = 0;
    };
}

#endif /* defined(__ndnrtc__camera_capturer__) */
