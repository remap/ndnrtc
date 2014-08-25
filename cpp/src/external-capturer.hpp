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

#include "external-capturer.h"
#include "base-capturer.h"

namespace ndnrtc {
    class ExternalCapturer : public IExternalCapturer, public BaseCapturer
    {
    public:
        ExternalCapturer(const ParamsStruct& params);
        virtual ~ExternalCapturer();
        
        int init();
        int startCapture();
        int stopCapture();
        
        // IExternalCapturer interface
        int incomingBGRAFrame(unsigned char* bgraFrameData,
                               unsigned int frameSize);
        
    private:
        IRawFrameConsumer* frameConsumer_ = NULL;
        webrtc::I420VideoFrame convertedFrame_;
    };
}

#endif /* defined(__libndnrtc__external_capturer__) */
