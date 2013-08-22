//
//  camera-capturer.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/16/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__camera_capturer__
#define __ndnrtc__camera_capturer__

#include "webrtc.h"
#include "ndnrtc-common.h"

#include "nsThreadUtils.h"

namespace ndnrtc {
    class IRawFrameConsumer;
    
    class CameraCapturerParams : public NdnParams
    {
    public:
        // construction/desctruction
        
        // public methods go here
        unsigned int getDeviceID() { return 0; }
        unsigned int getWidth() { return 640; };
        unsigned int getHeight() { return 480; };
        unsigned int getFPS() { return 30; };
    }
    
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
        
        // interface conformance
        void OnIncomingCapturedFrame(const int32_t id,
                                     webrtc::I420VideoFrame& videoFrame);
        void OnCaptureDelayChanged(const int32_t id,
                                   const int32_t delay);
//        void startCatpturing();
        
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
