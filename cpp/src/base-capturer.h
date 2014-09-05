//
//  capturer.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __libndnrtc__capturer__
#define __libndnrtc__capturer__

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

namespace ndnrtc {
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(webrtc::I420VideoFrame &frame,
                                    double unixTimeStamp) = 0;
    };
    
    class BaseCapturer : public NdnRtcObject {
    public:
        BaseCapturer(const ParamsStruct& params);
        virtual ~BaseCapturer();
        
        virtual int init();
        virtual int startCapture();
        virtual int stopCapture();
        
        bool isCapturing() { return isCapturing_; }
        
        void setFrameConsumer(IRawFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; }
        double getCapturingFrequency() { return NdnRtcUtils::currentFrequencyMeterValue(meterId_); }

    protected:
        bool isCapturing_ = false;
        IRawFrameConsumer *frameConsumer_ = nullptr;
        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> capture_cs_;
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &captureThread_;
        webrtc::EventWrapper &captureEvent_;
        webrtc::I420VideoFrame capturedFrame_, deliverFrame_;

        // statistics
        unsigned int meterId_;
        
        static bool processCapturedFrame(void *obj) { return ((BaseCapturer*)obj)->process(); }
        
        bool process();
        void deliverCapturedFrame(webrtc::I420VideoFrame& frame);
    };
}

#endif /* defined(__libndnrtc__capturer__) */
