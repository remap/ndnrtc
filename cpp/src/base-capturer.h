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

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "webrtc.h"
#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

namespace ndnrtc {
    
    class IRawFrameConsumer
    {
    public:
        virtual void onDeliverFrame(WebRtcVideoFrame &frame,
                                    double unixTimeStamp) = 0;
    };
    
    class BaseCapturer : public new_api::NdnRtcComponent {
    public:
        BaseCapturer();
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
        double lastFrameTimestamp_;
        
        boost::mutex captureMutex_, deliverMutex_;
        boost::thread captureThread_;
        boost::condition_variable_any deliverEvent_;
        WebRtcVideoFrame capturedFrame_, deliverFrame_;

        // statistics
        unsigned int meterId_;
        
        static bool processCapturedFrame(void *obj) { return ((BaseCapturer*)obj)->process(); }
        
        bool process();
        void deliverCapturedFrame(WebRtcVideoFrame& frame);
    };
}

#endif /* defined(__libndnrtc__capturer__) */
