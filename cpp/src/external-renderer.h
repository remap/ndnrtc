//
//  external-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef ndnrtc_external_renderer_h
#define ndnrtc_external_renderer_h

namespace ndnrtc {
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
         * This method is called every time new frame is available for rendering
         * @param timestamp Frame's timestamp
         * @param width Frame's width (NOTE: width can change during run)
         * @param height Frame's height (NOTE: height can change during run)
         * @param buffer Buffer with the RGB frame data (the same that was 
         * returned from getFrameBuffer call)
         */
        virtual void renderRGBFrame(int64_t timestamp, int width, int height,
                                    const uint8_t* buffer) = 0;
    };
}

#endif
