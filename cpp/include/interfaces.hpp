//
//  interfaces.hpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef libndnrtc_interfaces_h
#define libndnrtc_interfaces_h

#include <stdint.h>
#include "params.hpp"

namespace ndnrtc
{
    namespace statistics {
       class StatisticsStorage;
    }
    
    /**
     * This interface defines external renderers.
     * Each time, the frame is ready to be rendered, library calls
     * getFrameBuffer which should return a pointer to the buffer where library
     * can copy RGB frame data. Once this data is copied, library makes
     * renderRGBFrame call and passes the same buffer with additional parameters
     * so the renderer can perform rendering operations.
     */
    class IExternalRenderer
    {
    public:
        /**
         * Should return allocated buffer big enough to store RGB frame data
         * (width*height*3) bytes.
         * @param width Width of the frame (NOTE: width can change during run)
         * @param height Height of the frame (NOTE: height can change during run)
         * @return Allocated buffer where library can copy RGB frame data
         */
        virtual uint8_t* getFrameBuffer(int width, int height) = 0;
        
        /**
         * This method is called every time new frame is available for rendering.
         * This method is called on the same thread as getFrameBuffer was called.
         * @param timestamp Frame's timestamp
         * @param width Frame's width (NOTE: width can change during run)
         * @param height Frame's height (NOTE: height can change during run)
         * @param buffer Buffer with the RGB frame data (the same that was
         * returned from getFrameBuffer call)
         * @see getFrameBuffer
         */
        virtual void renderBGRAFrame(int64_t timestamp, int width, int height,
                                     const uint8_t* buffer) = 0;
    };

    /**
     * This class is used for delivering raw ARGB frames to the library.
     * After calling initPublishing, library returns a pointer of object
     * confirming this interface to a caller. Caller should use this pointer
     * for delivering frames into the library.
     * @see NdnRtcLibrary::initPublishing
     */
    class IExternalCapturer
    {
    public:
        /**
         * Calling this methond results in sending new raw frame into library's
         * video processing pipe which eventually should result in publishing
         * of encoded frame in NDN.
         * However, not every frame will be published - some frames are dropped
         * by the encoder.
         * @param bgraFramData Frame data in ARGB format
         * @param frameSize Size of the frame data
         */
        virtual int incomingArgbFrame(const unsigned int width,
                                      const unsigned int height,
                                      unsigned char* argbFrameData,
                                      unsigned int frameSize,
                                      unsigned int userDataSize = 0,
                                      unsigned char* userData = nullptr) = 0;
        
        /**
         * Alternative method for delivering YUV frames (I420 or y420 or 
         * Y'CbCr 8-bit 4:2:0) inside the library. Either of two "incoming"
         * calls should be used.
         */
        virtual int incomingI420Frame(const unsigned int width,
                                      const unsigned int height,
                                      const unsigned int strideY,
                                      const unsigned int strideU,
                                      const unsigned int strideV,
                                      const unsigned char* yBuffer,
                                      const unsigned char* uBuffer,
                                      const unsigned char* vBuffer,
                                      unsigned int userDataSize = 0,
                                      unsigned char* userData = nullptr) = 0;
    };
}

#endif
