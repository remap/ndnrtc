//
//  external-capturer.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef libndnrtc_external_capturer_h
#define libndnrtc_external_capturer_h

namespace ndnrtc
{
    /**
     * This class is used for delivering raw BGRA frames to the library. 
     * After calling initPublishing, library returns a pointer of object 
     * confirming this interface to a caller. Caller should use this pointer 
     * for delivering frames into the library.
     * @see NdnRtcLibrary::initPublishing
     */
    class IExternalCapturer
    {
    public:
        /**
         * This method should be called in order to initiate frame delivery into
         * the library
         */
        virtual void capturingStarted() = 0;
        
        /**
         * This method should be called in order to stop frame delivery into the 
         * library
         */
        virtual void capturingStopped() = 0;
        
        /**
         * Calling this methond results in sending new raw frame into library's 
         * video processing pipe which eventually should result in publishing
         * of encoded frame in NDN. 
         * However, not every frame will be published - some frames are dropped 
         * by the encoder.
         * @param bgraFramData Frame data in BGRA format
         * @param frameSize Size of the frame data
         */
        virtual int incomingArgbFrame(unsigned char* argbFrameData,
                                      unsigned int frameSize) = 0;
    };
}

#endif
