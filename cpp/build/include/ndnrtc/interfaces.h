//
//  interfaces.h
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
#include "params.h"

namespace ndnrtc
{
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
         * @param bgraFramData Frame data in ARGB format
         * @param frameSize Size of the frame data
         */
        virtual int incomingArgbFrame(const unsigned int width,
                                      const unsigned int height,
                                      unsigned char* argbFrameData,
                                      unsigned int frameSize) = 0;
        
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
                                      const unsigned char* vBuffer) = 0;
    };
    
    /**
     * Session observer
     */
    typedef enum _SessionStatus {
        SessionOffline = 0,
        SessionOnlineNotPublishing = 1,
        SessionOnlinePublishing = 2
    } SessionStatus;
    
    class ISessionObserver {
    public:
        
        virtual void
        onSessionStatusUpdate(const char* username, const char* sessionPrefix,
                              SessionStatus status) = 0;
        
        virtual void
        onSessionError(const char* username, const char* sessionPrefix,
                       SessionStatus status, unsigned int errorCode,
                       const char* errorMessage) = 0;
        
        virtual void
        onSessionInfoUpdate(const new_api::SessionInfo& sessionInfo) = 0;
    };
    
    class IRemoteSessionObserver {
    public:
        virtual void
        onSessionInfoUpdate(const new_api::SessionInfo& sessionInfo) = 0;
        
        virtual void
        onUpdateFailedWithTimeout() = 0;
        
        virtual void
        onUpdateFailedWithError(int errCode, const char* errMsg) = 0;
    };
    
    class INdnRtcObjectObserver {
    public:
        virtual ~INdnRtcObjectObserver(){}
        virtual void onErrorOccurred(const char *errorMessage) = 0;
    };
    
    /**
     * This abstract class declares interface for the library's observer - an
     * instance which can receive status updates from the library.
     */
    class INdnRtcLibraryObserver {
    public:
        /**
         * This method is called whenever library encouteres errors or state
         * transistions (for instance, fetching has started). Arguments provided
         * work only as a source of additional information about what has
         * happened inside the library.
         * @param state Indicates which state library has encountered (i.e.
         * "error" or "info"). Can be used by observer for filtering important
         * events. Currently, following states are provided:
         *      "error"
         *      "info"
         * @param args Any additional info that accompany new state (human-
         * readable text information).
         */
        virtual void onStateChanged(const char *state, const char *args) = 0;
        
        /**
         * Called when errors occur.
         * @param errorCode Error code (@see error-codes.h)
         * @param message Error message
         */
        virtual void onErrorOccurred(int errorCode, const char* message) = 0;
    };
    
    /**
     * Interface for remote stream observer. Gets updates for notable events 
     * occuring while remote stream is fetched.
     */
    typedef enum _ConsumerStatus {
        ConsumerStatusStopped,
        ConsumerStatusNoData,   // consumer has started but no data received yet
        ConsumerStatusAdjusting,  // consumer re-adjusts interests' window
        ConsumerStatusBuffering,    // consumer has finished chasing and is
                                    // buffering frames unless buffer reaches
                                    // target size
        ConsumerStatusFetching, // consumer has finished buffering and switched
                                // to normal operating fetching mode
    } ConsumerStatus;
    
    // playback events occur at the moment when frame is taken from the buffer
    // for playback by playback mechanism. this means, the frame taken has
    // reached it's deadline and should be played back or skipped due to
    // different reasons:
    typedef enum _PlaybackEvent {
        PlaybackEventDeltaSkipIncomplete, // consumer had to skip frame as it was
                                          // not fully fetched (incomplete)
        PlaybackEventDeltaSkipInvalidGop, // consumer had to skip frame due to
                                          // receiving incomplete frame previously,
                                          // even if the current frame is complete
        PlaybackEventDeltaSkipNoKey,      // consumer had to skip frame as there
                                          // is no key frame for frame's GOP
        PlaybackEventKeySkipIncomplete    // consumer had to skip key frame as
                                          // it is incomplete
    } PlaybackEvent;
    
    class IConsumerObserver {
    public:
        /**
         * Called when consumer updates its' status
         */
        virtual void
        onStatusChanged(ConsumerStatus newStatus) = 0;
        
        /**
         * Called each time consumer encounters rebuffering event (no data 
         * received for ~1200ms)
         */
        virtual void
        onRebufferingOccurred() = 0;
        
        /**
         * Called each time consumer encounters new buffer event
         */
        virtual void
        onPlaybackEventOccurred(PlaybackEvent event, unsigned int frameSeqNo) = 0;
        
        /**
         * Called when stream has switched to another thread
         */
        virtual void
        onThreadSwitched(const std::string& threadName) = 0;
    };
}

#endif
