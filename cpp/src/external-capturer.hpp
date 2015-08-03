//
//  external-capturer.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __libndnrtc__external_capturer__
#define __libndnrtc__external_capturer__

#include "interfaces.h"
#include "base-capturer.h"

namespace ndnrtc {
    class ExternalCapturer : public BaseCapturer, public IExternalCapturer
    {
    public:
        ExternalCapturer();
        virtual ~ExternalCapturer();
        
        int init();
        
        // IExternalCapturer interface
        void capturingStarted();
        void capturingStopped();
        
        // either of incomingArgbFrame or incomingYuvFrame calls should be used
        int incomingArgbFrame(const unsigned int width,
                              const unsigned int height,
                              unsigned char* argbFrameData,
                              unsigned int frameSize);

        // I420 or y420 or Y'CbCr 8-bit 4:2:0 format
        int incomingI420Frame(const unsigned int width,
                              const unsigned int height,
                              const unsigned int strideY,
                              const unsigned int strideU,
                              const unsigned int strideV,
                              const unsigned char* yBuffer,
                              const unsigned char* uBuffer,
                              const unsigned char* vBuffer);
        
    private:
        int64_t incomingTimestampMs_;
        WebRtcVideoFrame convertedFrame_;
        
        int startCapture();
        int stopCapture();
    };
}

#endif /* defined(__libndnrtc__external_capturer__) */
