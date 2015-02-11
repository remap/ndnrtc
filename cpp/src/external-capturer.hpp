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
        int incomingArgbFrame(const unsigned int width,
                              const unsigned int height,
                              unsigned char* argbFrameData,
                              unsigned int frameSize);
        
    private:
        int64_t incomingTimestampMs_;
        webrtc::I420VideoFrame convertedFrame_;
        
        int startCapture();
        int stopCapture();
    };
}

#endif /* defined(__libndnrtc__external_capturer__) */
