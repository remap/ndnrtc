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
#include <ndn-cpp/security/key-chain.hpp>

#include "params.h"


namespace boost { namespace asio { class io_service; }}

namespace ndnrtc
{
    namespace new_api
    {
        namespace statistics {
            class StatisticsStorage;
        }
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
        SessionOnlinePublishing = 2,
        SessionInvalidated = 3
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
    
    /******************************************************************************
     * This is an interface for the NDN-RTC library
     ******************************************************************************/
    class INdnRtcLibrary {
    public:
        /**
         * Sets library observer
         * @param observer Pointer to the observer object
         * @see INdnRtcLibraryObserver
         */
        virtual void
        setObserver(INdnRtcLibraryObserver *observer) = 0;
        
        /**
         * Starts NDN-RTC session with username and parameters provided
         * @param username NDN-RTC user name
         * @param prefix Home hub prefix
         * @param generalParams General NDN-RTC parameters object
         * @param keyChain Key chain used to sign all generated data packets
         * @param sessionObserver Pointer to the session observer object
         * @return Session prefix in the following form:
         *      <prefix>/<ndnrtc_component>/<username>
         *      where ndnrtc_component is "ndnrtc/user", but may be changed in
         *      future releases
         *      prefix is taken from generalParams object
         *      If failed - returns empty string and error message is sent to
         *      library observer
         * @see setObserver
         */
        virtual std::string
        startSession(const std::string& username,
                     const std::string& prefix,
                     const new_api::GeneralParams& generalParams,
                     ndn::KeyChain* keyChain,
                     ndn::Face* face,
                     boost::asio::io_service& io,
                     ISessionObserver *sessionObserver) = 0;

        /**
         * Stops session started previously
         * @param sessionPrefix Session prefix returned from startSession call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see startSession
         */
        virtual int
        stopSession(const std::string& sessionPrefix) = 0;
        
        /**
         * Adds publishing stream to the session
         * @param sessionPrefix Session prefix returned by previous startSession
         *                      call
         * @param params Media stream parameters
         * @param capturer Upon success, pointer contains a pointer to the object,
         *                 which is used as an external video capturing system
         *                 (video only). In this case, stream publishing is driven
         *                 by this object - whenever new frame is fed to this
         *                 object, it gets published
         * @return Stream prefix in th following form:
         *         <session_prefix>/<stream_name>
         *         If failed, returns empty string
         * @see startSession, IExternalCapturer
         */
        virtual std::string
        addLocalStream(const std::string& sessionPrefix,
                       const new_api::MediaStreamParams& params,
                       IExternalCapturer** const capturer) = 0;
        
        /**
         * Removes publishing stream
         * @param sessionPrefix Session prefix returned by previous startSession
         *                      call
         * @param streamPrefix Stream prefix returned by previous addLocalStream
         *                     call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see startSession, addLocalStream
         */
        virtual int
        removeLocalStream(const std::string& sessionPrefix,
                          const std::string& streamPrefix) = 0;
        
        /**
         * Adds new remote stream and starts fetching from its' first thread
         * @param remoteSessionPrefix Remote producer's session prefix returned
         *                            by previsous setRemoteSessionObserver call
         * @param threadName Thread name to fetch from
         * @param params Media stream parameters
         * @param generalParams General NDN-RTC parameters
         * @param consumerParams General consumer parameters
         * @param keyChain Key chain used to verify data
         * @param renderer Pointer to the object which conforms to the
         *                 IExternalRenderer interface (video only)
         * @return Remote stream prefix on success and empty string on failure
         * @see setRemoteSessionObserver
         */
        virtual std::string
        addRemoteStream(const std::string& remoteSessionPrefix,
                        const std::string& threadName,
                        const new_api::MediaStreamParams& params,
                        const new_api::GeneralParams& generalParams,
                        const new_api::GeneralConsumerParams& consumerParams,
                        ndn::KeyChain* keyChain,
                        IExternalRenderer* const renderer) = 0;
        
        /**
         * Removes remote stream and stops fetching from it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @return Full log file path on success, empty string on failure
         */
        virtual std::string
        removeRemoteStream(const std::string& streamPrefix) = 0;
        
        /**
         * Sets remote stream obsever and starts sending fetching updates to it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param observer Pointer to an object which conforms to the
         *                 IConsumerObserver interface
         * @return RESULT_OK on success, RESULT_ERR on failure
         */
        virtual int
        setStreamObserver(const std::string& streamPrefix,
                          IConsumerObserver* const observer) = 0;
        
        /**
         * Removes remote stream observer and stops sending updates to it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see setStreamObserver
         */
        virtual int
        removeStreamObserver(const std::string& streamPrefix) = 0;
        
        /**
         * Returns currently fetched media thread name
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @see addRemoteStream
         */
        virtual std::string
        getStreamThread(const std::string& streamPrefix) = 0;
        
        /**
         * Forces specified fetching stream to switch active media thread
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param threadName Name of the thread which should be used for
         *                   fetching media. All threads are returned in SessionInfo
         *                   object to remote session observer
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see setRemoteSessionObserver
         */
        virtual int
        switchThread(const std::string& streamPrefix,
                     const std::string& threadName) = 0;
        
        /**
         * Returns statistics of remote fetching stream
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param stat Reference to a receiver statistics object
         * @return RESULT_OK on success, RESULT_ERR on failure
         */
        virtual new_api::statistics::StatisticsStorage
        getRemoteStreamStatistics(const std::string& streamPrefix) = 0;
        
        /**
         * Returns current library version
         * @param versionString A pointer to the string where the library
         * version will be stored.
         */
        virtual void getVersionString(char **versionString) = 0;
        
        /**
         * Serializes session info structure into an array of bytes.
         * @param sessionInfo Session info structure to serialize
         * @param length Upon succesfull serialization, contains lenght of the
         *   byte array, otherwise - 0
         * @param bytes Contains serialized bytes array for session info structure
         * @note Bytes array is allocated inside this function. One should check
         * length parameter - if it's greater than 0, deallocation of bytes array
         * is the responsibility of the caller.
         */
        virtual void serializeSessionInfo(const new_api::SessionInfo &sessionInfo,
                                          unsigned int &length,
                                          unsigned char **bytes) = 0;
        
        /**
         * Deserializes bytes array into a session info structure
         * @param length Length of the bytes array
         * @param btyes Bytes array containing session info strcture bytes
         * @param sessionInfo Deserialized session info structure
         * @return true if deserialization was successeful, false otherwise
         */
        virtual bool deserializeSessionInfo(const unsigned int length,
                                            const unsigned char *bytes,
                                            new_api::SessionInfo &sessionInfo) = 0;
    };
}

#endif
